#include "PitchEngine.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace joji {
namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

// ── Location intents ──────────────────────────────────────────────────────
enum class LocationIntent { LowOutside, HighInside, ChaseDown, ChaseOut, HeartZone };

struct Candidate {
    PitchType type;
    LocationIntent intent;
    int grade;
    double score;
};

// Returns (targetX, targetZ) for a given intent.
// awayDir: +1 = catcher's right is "away" (RHB), -1 (LHB).
std::pair<double, double> intentTarget(LocationIntent intent, double awayDir) {
    switch (intent) {
        case LocationIntent::LowOutside: return {awayDir * 0.50, 1.75};
        case LocationIntent::HighInside: return {-awayDir * 0.60, 3.15};
        case LocationIntent::ChaseDown:  return {awayDir * 0.15, 0.75};
        case LocationIntent::ChaseOut:   return {awayDir * 1.25, 2.20};
        case LocationIntent::HeartZone:  return {0.0, 2.50};
    }
    return {0.0, 2.50};
}

bool isInZone(LocationIntent i) {
    return i == LocationIntent::LowOutside
        || i == LocationIntent::HighInside
        || i == LocationIntent::HeartZone;
}
bool isChase(LocationIntent i) {
    return i == LocationIntent::ChaseDown || i == LocationIntent::ChaseOut;
}

// ── Score a single candidate ─────────────────────────────────────────────
double scoreCandidate(
    const Player& batter, const Count& count,
    const std::optional<Pitch>& lastPitch,
    const std::optional<BatterHistory>& history,
    PitchType type, LocationIntent intent, int grade,
    double awayDir, Random& random)
{
    const bool breaking  = (type == PitchType::Slider || type == PitchType::Curveball || type == PitchType::Splitter);
    const bool offspeed  = (type == PitchType::Changeup);
    const bool fastball  = (type == PitchType::Fastball || type == PitchType::Cutter);
    const bool inZone    = isInZone(intent);
    const bool chase     = isChase(intent);
    const bool heart     = intent == LocationIntent::HeartZone;

    // Pitcher grade bonus: grade 70 = +0.16, grade 40 = -0.08
    double score = (grade - 50) / 50.0 * 0.40;

    // ── Count ────────────────────────────────────────────────────────────
    if (count.balls == 3 && count.strikes == 0) {
        // 3-0: strong zone preference, fastball first — walk is costly
        if (chase)              score -= 2.0;
        if (inZone)             score += 0.9;
        if (fastball && inZone) score += 0.5;
        if (heart)              score += 0.3;
    } else if (count.balls >= 3) {
        // 3-1, 3-2: still need a strike, but off-speed is usable
        if (chase)  score -= 1.8;
        if (inZone) score += 0.9;
        if (heart)  score += 0.4;
        if (fastball && inZone) score += 0.3;
    } else if (count.strikes >= 2 && count.balls <= 1) {
        // Pitcher's count — expand zone, can waste a pitch
        if (chase)  score += 0.8;
        if (heart)  score -= 0.5;
    } else if (count.balls >= 2 && count.strikes <= 1) {
        // Hitter's count — don't give a fat one
        if (heart)  score -= 0.7;
        if (chase)  score -= 0.4;
        if (intent == LocationIntent::LowOutside) score += 0.3;
    } else {
        // Neutral count — slight preference for quality strike locations
        if (inZone && !heart) score += 0.2;
        // 0-0 first pitch: establish fastball strike, set the tone early
        if (count.balls == 0 && count.strikes == 0 && fastball && inZone) score += 0.25;
    }

    // ── Batter tendencies ────────────────────────────────────────────────
    const double eyeAdj   = clamp((batter.eye        - 50) / 50.0, -0.8, 0.8);
    const double chaseAdj = clamp((batter.chaseRate   - 50) / 50.0, -0.8, 0.8);
    const double cvbAdj   = clamp((batter.contactVsBreaking - 50) / 50.0, -0.8, 0.8);
    const double hbhAdj   = clamp((batter.highBallHitter    - 50) / 50.0, -0.8, 0.8);
    const double powerAdj = clamp((batter.power       - 50) / 50.0, -0.8, 0.8);

    // High chase rate → off-zone pitches more effective (they'll bite)
    if (chase) score += chaseAdj * 0.45 - eyeAdj * 0.12;

    // Breaking balls: in pitcher's count (0-2/1-2) lean harder on matchup advantage —
    // if batter hits breaking balls well, pivot to fastball; if they struggle, go to it.
    const double cvbWeight = (count.strikes >= 2 && count.balls <= 1) ? 0.50 : 0.35;
    if (breaking) score -= cvbAdj * cvbWeight;

    // High-ball hitter: attack low; chase down works as chase
    if (intent == LocationIntent::HighInside && hbhAdj > 0) score -= hbhAdj * 0.30;
    if (intent == LocationIntent::LowOutside  && hbhAdj < 0) score -= (-hbhAdj) * 0.20;
    if (intent == LocationIntent::ChaseDown   && hbhAdj > 0) score += hbhAdj * 0.35;

    // Long ball risk: power hitter + center zone = disaster
    if (heart && powerAdj > 0)                score -= powerAdj * 0.75;
    if (inZone && fastball && powerAdj > 0.3) score -= powerAdj * 0.20;

    // Changeup is less effective vs high-pull / power batters if it's center
    if (offspeed && heart) score -= 0.15;

    // ── Previous pitch setup ─────────────────────────────────────────────
    if (lastPitch.has_value()) {
        const PitchType prev = lastPitch->pitchType;

        // Penalty for repeating same pitch type
        if (prev == type) score -= 0.35;

        // Setup combos: classic sequences get a bonus
        if ((prev == PitchType::Fastball || prev == PitchType::Cutter)
            && (breaking || offspeed)) score += 0.28;

        if ((prev == PitchType::Slider || prev == PitchType::Curveball)
            && fastball && intent == LocationIntent::HighInside) score += 0.20;

        if (prev == PitchType::Changeup && fastball) score += 0.15;

        // Location variety: in ↔ out sequencing
        const bool lastWasAway = lastPitch->locationX * awayDir > 0.25;
        if (lastWasAway && intent == LocationIntent::HighInside) score += 0.18;
        if (!lastWasAway && intent == LocationIntent::LowOutside) score += 0.18;
    }

    // ── Cross-at-bat memory: batter's in-game tendencies ────────────────────
    // If batter chased or whiffed this pitch type in a previous AB, lean on it.
    // If batter consistently laid off a chase pitch type, reduce its use as waste pitch.
    if (history.has_value()) {
        const std::string typeName = toString(type);
        const auto& h = *history;
        const auto chIt = h.chases.find(typeName);
        const auto whIt = h.whiffs.find(typeName);
        if (chIt != h.chases.end() && chIt->second > 0)  score += 0.18;
        if (whIt != h.whiffs.end() && whIt->second > 0)  score += 0.12;
        if (chase && chIt != h.chases.end() && chIt->second == 0 && h.totalPitches >= 4)
            score -= 0.10;
    }

    score += random.real(-0.35, 0.35);
    return score;
}

// ── Physics params per pitch type ────────────────────────────────────────
double velocityFor(PitchType type, const Player& pitcher, Random& random) {
    const double base = 88.0 + (pitcher.pitchingVelocity - 50) * 0.16;
    switch (type) {
        case PitchType::Fastball:  return base + random.real(-1.8, 2.2);
        case PitchType::Cutter:    return base - 3.0 + random.real(-1.8, 1.8);
        case PitchType::Slider:    return base - 6.5 + random.real(-2.0, 2.0);
        case PitchType::Changeup:  return base - 9.5 + random.real(-2.5, 2.0);
        case PitchType::Splitter:  return base - 7.5 + random.real(-2.3, 2.0);
        case PitchType::Curveball: return base - 12.0 + random.real(-2.5, 2.5);
    }
    return base;
}

void applySpinParams(Pitch& pitch, double handSign, const Player& pitcher, Random& random) {
    const double stuff = clamp((pitcher.pitchingStuff - 50) / 50.0, -0.8, 0.8);
    // armDrag: pitchers with lower control have more gyro spin from arm-slot deviation.
    // This makes SSW (seam-shift wake) more unpredictable on their pitches.
    const double armDrag = clamp((55.0 - pitcher.pitchingControl) / 60.0, 0.0, 0.42);
    switch (pitch.pitchType) {
        case PitchType::Fastball:
            pitch.movementX  = random.real(-3.0, 3.0);
            pitch.movementZ  = random.real(7.0, 15.0) + stuff * 3.0;
            pitch.spinRate   = random.real(2050.0, 2550.0);
            pitch.spinAxisX  = -0.97; pitch.spinAxisY = armDrag * 0.18;        pitch.spinAxisZ = 0.26 * handSign;
            pitch.activeSpin = 0.93;
            break;
        case PitchType::Slider:
            pitch.movementX  = handSign * random.real(5.0, 14.0);
            pitch.movementZ  = random.real(-2.0, 5.0);
            pitch.spinRate   = random.real(2250.0, 2900.0);
            pitch.spinAxisX  = 0.25;  pitch.spinAxisY = 0.15 + armDrag * 0.22; pitch.spinAxisZ = 0.97 * handSign;
            pitch.activeSpin = 0.35;
            break;
        case PitchType::Curveball:
            pitch.movementX  = handSign * random.real(-5.0, 5.0);
            pitch.movementZ  = random.real(-16.0, -7.0);
            pitch.spinRate   = random.real(2350.0, 3050.0);
            pitch.spinAxisX  = 0.97;  pitch.spinAxisY = armDrag * 0.22;        pitch.spinAxisZ = -0.26 * handSign;
            pitch.activeSpin = 0.90;
            break;
        case PitchType::Changeup:
            pitch.movementX  = handSign * random.real(-7.0, 5.0);
            pitch.movementZ  = random.real(0.0, 8.0);
            pitch.spinRate   = random.real(1500.0, 2100.0);
            pitch.spinAxisX  = -0.92; pitch.spinAxisY = 0.22 + armDrag * 0.28; pitch.spinAxisZ = 0.40 * handSign;
            pitch.activeSpin = 0.70;
            break;
        case PitchType::Cutter:
            pitch.movementX  = handSign * random.real(2.0, 8.0);
            pitch.movementZ  = random.real(4.0, 11.0);
            pitch.spinRate   = random.real(2200.0, 2750.0);
            pitch.spinAxisX  = -0.87; pitch.spinAxisY = 0.10 + armDrag * 0.25; pitch.spinAxisZ = -0.50 * handSign;
            pitch.activeSpin = 0.50;
            break;
        case PitchType::Splitter:
            pitch.movementX  = handSign * random.real(-4.0, 4.0);
            pitch.movementZ  = random.real(-9.0, -2.0);
            pitch.spinRate   = random.real(1200.0, 1800.0);
            pitch.spinAxisX  = 0.60;  pitch.spinAxisY = 0.28 + armDrag * 0.18; pitch.spinAxisZ = 0.0;
            pitch.activeSpin = 0.25;
            break;
    }
    // Renormalize to unit vector after adding gyro (spinAxisY) component
    const double len = std::sqrt(pitch.spinAxisX * pitch.spinAxisX
                               + pitch.spinAxisY * pitch.spinAxisY
                               + pitch.spinAxisZ * pitch.spinAxisZ);
    if (len > 0.01) {
        pitch.spinAxisX /= len;
        pitch.spinAxisY /= len;
        pitch.spinAxisZ /= len;
    }
}

} // namespace

// ── PitchEngine::generate ─────────────────────────────────────────────────
Pitch PitchEngine::generate(const Player& pitcher, const Player& batter,
                            const Count& count, const std::optional<Pitch>& lastPitch,
                            const std::optional<BatterHistory>& history,
                            Random& random) const
{
    const BattingSide side = batter.battingSide;
    // awayDir: +1 = catcher's right is "away" for RHB; -1 for LHB; default RHB for Switch.
    const double awayDir = (side == BattingSide::Left) ? -1.0 : 1.0;
    const double handSign = (pitcher.throwingHand == ThrowingHand::Right) ? 1.0 : -1.0;

    // ── Build candidate list ──────────────────────────────────────────────
    // Use arsenal if defined, otherwise fall back to plausible defaults.
    std::vector<PitchGrade> grades = pitcher.arsenal;
    if (grades.empty()) {
        // Default: fastball-heavy with platoon-aware secondary
        const bool sameHanded =
            (pitcher.throwingHand == ThrowingHand::Right && side == BattingSide::Right)
         || (pitcher.throwingHand == ThrowingHand::Left  && side == BattingSide::Left);
        grades = {
            {PitchType::Fastball,  60},
            {sameHanded ? PitchType::Slider : PitchType::Changeup, 50},
            {PitchType::Curveball, 48},
        };
    }

    // All intents are candidates; scoring will filter naturally.
    const LocationIntent intents[] = {
        LocationIntent::LowOutside,
        LocationIntent::HighInside,
        LocationIntent::ChaseDown,
        LocationIntent::ChaseOut,
        LocationIntent::HeartZone,
    };

    Candidate best;
    best.score = -1e9;

    for (const auto& pg : grades) {
        for (LocationIntent intent : intents) {
            Candidate c;
            c.type   = pg.type;
            c.intent = intent;
            c.grade  = pg.grade;
            c.score  = scoreCandidate(batter, count, lastPitch, history,
                                       pg.type, intent, pg.grade,
                                       awayDir, random);
            if (c.score > best.score) best = c;
        }
    }

    // ── Build pitch from best candidate ───────────────────────────────────
    Pitch pitch;
    pitch.pitchType     = best.type;
    pitch.pitchVelocity = clamp(velocityFor(best.type, pitcher, random), 68.0, 102.0);

    const double control = clamp((pitcher.pitchingControl - 50) / 50.0, -0.8, 0.8);
    // In ball-heavy counts pitchers try for the zone but still miss under pressure.
    // This keeps 3-ball command explainable while allowing a league-average walk rate.
    const double behindInCount = count.balls >= 3 ? 0.07 : 0.0;
    const double aheadInCount  = count.strikes >= 2 ? 0.10 : 0.0;
    // commandSpread: distance from target that command noise can reach.
    const double commandSpread = clamp(0.60 - control * 0.22 - behindInCount + aheadInCount, 0.26, 0.90);

    auto [tx, tz] = intentTarget(best.intent, awayDir);
    pitch.locationX = clamp(tx + random.real(-commandSpread, commandSpread), -2.0, 2.0);
    pitch.locationZ = clamp(tz + random.real(-commandSpread * 0.8, commandSpread * 0.8), 0.3, 5.0);

    applySpinParams(pitch, handSign, pitcher, random);

    const double edgePenalty = std::abs(pitch.locationX) * 0.15 + std::abs(pitch.locationZ - 2.5) * 0.08;
    const double stuff = clamp((pitcher.pitchingStuff - 50) / 50.0, -0.8, 0.8);
    pitch.pitchQuality = clamp(0.52 + control * 0.26 + stuff * 0.26
                                - edgePenalty + random.real(-0.12, 0.12), 0.0, 1.0);
    return pitch;
}

} // namespace joji
