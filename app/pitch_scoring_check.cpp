#include "AtBatEngine.h"
#include "BallPhysicsEngine.h"
#include "ContactEngine.h"
#include "PitchEngine.h"
#include "Player.h"
#include "Random.h"
#include "SwingDecisionEngine.h"
#include "SwingEngine.h"
#include "Teams.h"
#include "ZoneJudge.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace joji;

std::string pitchName(PitchType t) {
    switch (t) {
        case PitchType::Fastball:  return "FB";
        case PitchType::Slider:    return "SL";
        case PitchType::Curveball: return "CB";
        case PitchType::Changeup:  return "CH";
        case PitchType::Cutter:    return "CT";
        case PitchType::Splitter:  return "SP";
    }
    return "??";
}

bool isInZone(const Pitch& p) {
    return std::abs(p.locationX) <= 0.83 && p.locationZ >= 1.55 && p.locationZ <= 3.50;
}

struct BatterProfile {
    std::string label;
    Player batter;
};

struct PitcherProfile {
    std::string label;
    Player pitcher;
};

// Run N at-bats and collect pitch breakdown
void runProfile(const PitcherProfile& pp, const BatterProfile& bp,
                int atBats, std::uint64_t seed)
{
    PitchEngine pitchEng;
    AtBatEngine engine{BallPhysicsEngine{}, pitchEng, SwingDecisionEngine{},
                       ZoneJudge{}, SwingEngine{}, ContactEngine{}};
    Random rng{seed};

    std::map<std::string, int> typeCounts;
    int totalPitches = 0;
    int chases = 0;          // swings at off-zone pitches
    int offZonePitches = 0;
    int heartZonePitches = 0; // |x| < 0.3 && 2.1 < z < 2.9
    int swings = 0;

    for (int g = 0; g < atBats; ++g) {
        AtBatState state = engine.startAtBat(bp.batter, pp.pitcher);
        while (!state.isFinished) {
            engine.simulateNextPitch(state, rng);
        }
        for (const auto& log : state.pitchLogs) {
            typeCounts[pitchName(log.pitch.pitchType)]++;
            totalPitches++;
            const bool zone = isInZone(log.pitch);
            const bool heart = std::abs(log.pitch.locationX) < 0.30
                            && log.pitch.locationZ > 2.1
                            && log.pitch.locationZ < 2.9;
            if (heart) heartZonePitches++;
            if (!zone) {
                offZonePitches++;
                if (log.swingDecision.decision == SwingDecisionType::Swing) chases++;
            }
            if (log.swingDecision.decision == SwingDecisionType::Swing) swings++;
        }
    }

    std::cout << "\n[" << pp.label << "] vs [" << bp.label << "]  (" << atBats << " AB, "
              << totalPitches << " pitches)\n";
    std::cout << "  PitchMix: ";
    for (auto& [k, v] : typeCounts) {
        std::cout << k << "=" << (v * 100 / totalPitches) << "% ";
    }
    std::cout << "\n";
    std::cout << "  HeartZone%: " << (heartZonePitches * 100 / totalPitches)
              << "   OffZone%: " << (offZonePitches * 100 / totalPitches)
              << "   ChaseRate(offzone swing): "
              << (offZonePitches > 0 ? (chases * 100 / offZonePitches) : 0) << "%\n";
    std::cout << "  SwingRate: " << (totalPitches > 0 ? (swings * 100 / totalPitches) : 0) << "%\n";
}

Player makePitcher(const std::string& name, int vel, int ctrl, int stuff,
                   std::vector<PitchGrade> arsenal)
{
    Player p;
    p.name = name; p.position = Position::Pitcher;
    p.pitchingVelocity = vel; p.pitchingControl = ctrl; p.pitchingStuff = stuff;
    p.pitchingStamina = 60; p.throwingHand = ThrowingHand::Right;
    p.arsenal = std::move(arsenal);
    return p;
}

Player makeBatter(const std::string& name, int contact, int power, int eye,
                  int pull, int hb, int chase, int cvb,
                  BattingSide side = BattingSide::Right)
{
    Player p;
    p.name = name; p.position = Position::CenterField;
    p.contact = contact; p.power = power; p.eye = eye; p.speed = 55;
    p.pullTendency = pull; p.highBallHitter = hb;
    p.chaseRate = chase; p.contactVsBreaking = cvb;
    p.battingSide = side;
    return p;
}

} // namespace

int main() {
    using namespace joji;

    // ── 投手プロファイル ──────────────────────────────────────────
    PitcherProfile fastballPitcher{"FastballPitcher(FB75/SL60)", makePitcher(
        "Ace", 80, 68, 75,
        {{PitchType::Fastball,75},{PitchType::Slider,60},{PitchType::Changeup,50}}
    )};
    PitcherProfile breakingPitcher{"BreakingPitcher(FB60/CB72/CH65)", makePitcher(
        "Crafty", 65, 72, 72,
        {{PitchType::Fastball,60},{PitchType::Curveball,72},{PitchType::Changeup,65}}
    )};

    // ── 打者プロファイル ──────────────────────────────────────────
    // pull=68 chase=65 cvb=40 → 典型的パワーヒッター
    BatterProfile powerHitter{"PowerHitter(pow75,chase65,cvb40)",
        makeBatter("Power", 65, 75, 55, 68, 45, 65, 40)};
    // chase=30 eye=75 → 選球眼が良い
    BatterProfile eyeHitter{"EyeHitter(eye75,chase30)",
        makeBatter("Eye", 76, 58, 75, 44, 52, 30, 62)};
    // cvb=70 chase=38 → 変化球得意
    BatterProfile breakingHitter{"BreakingHitter(cvb70,chase38)",
        makeBatter("Contact", 78, 60, 68, 44, 54, 38, 70)};

    constexpr int N = 800;
    constexpr std::uint64_t SEED = 42;

    std::cout << "=== Pitch Scoring Verification (" << N << " AB each) ===\n";

    // パワーヒッターへの配球: FB少なく変化球多いか？ ハートゾーン少ないか？
    runProfile(fastballPitcher, powerHitter,   N, SEED);
    runProfile(fastballPitcher, eyeHitter,     N, SEED + 1);
    runProfile(fastballPitcher, breakingHitter,N, SEED + 2);

    runProfile(breakingPitcher, powerHitter,   N, SEED + 3);
    runProfile(breakingPitcher, eyeHitter,     N, SEED + 4);
    runProfile(breakingPitcher, breakingHitter,N, SEED + 5);

    std::cout << "\n=== Expected tendencies ===\n"
              << "  Power hitter:    HeartZone% LOW, more off-zone pitches\n"
              << "  Eye hitter:      ChaseRate LOW (won't bite), OffZone% LOW\n"
              << "  Breaking hitter: BreakingPitcher should throw MORE CB/CH vs others\n";
    return 0;
}
