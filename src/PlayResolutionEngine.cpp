#include "PlayResolutionEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace joji {

namespace {

constexpr double FeetPerMeter = 3.280839895;
constexpr double TimeStep = 0.016;

double meters(double value) {
    return value * FeetPerMeter;
}

double distance2d(const Vector3& left, const Vector3& right) {
    const double dx = left.x - right.x;
    const double dy = left.y - right.y;
    return std::sqrt(dx * dx + dy * dy);
}

double radialDistance(const Vector3& point) {
    return std::sqrt(point.x * point.x + point.y * point.y);
}

std::size_t indexForPosition(FieldPosition position) {
    return static_cast<std::size_t>(position);
}

// ── Candidate selectors ───────────────────────────────────────────────────

std::vector<std::size_t> groundCandidateIndices(const BattedBall& ball) {
    const double spray = ball.sprayAngle;
    const double d     = radialDistance(ball.landingPoint);

    if (d < 22.0) return {indexForPosition(FieldPosition::Catcher)};
    if (d < 34.0) return {indexForPosition(FieldPosition::Catcher),
                          indexForPosition(FieldPosition::Pitcher)};
    if (d < 70.0 && std::abs(spray) < 12.0)
        return {indexForPosition(FieldPosition::Pitcher), indexForPosition(FieldPosition::Catcher),
                indexForPosition(FieldPosition::Shortstop), indexForPosition(FieldPosition::SecondBase)};
    if (spray <= -28.0) return {indexForPosition(FieldPosition::ThirdBase)};
    if (spray <= -12.0) return {indexForPosition(FieldPosition::ThirdBase),
                                indexForPosition(FieldPosition::Shortstop)};
    if (spray <=   0.0) return {indexForPosition(FieldPosition::Shortstop),
                                indexForPosition(FieldPosition::ThirdBase),
                                indexForPosition(FieldPosition::SecondBase)};
    if (spray <=  15.0) return {indexForPosition(FieldPosition::SecondBase),
                                indexForPosition(FieldPosition::Shortstop),
                                indexForPosition(FieldPosition::FirstBase)};
    if (spray <=  28.0) return {indexForPosition(FieldPosition::SecondBase),
                                indexForPosition(FieldPosition::FirstBase)};
    return {indexForPosition(FieldPosition::FirstBase)};
}

// Pop-up / bloop: use infield + catcher candidates
std::vector<std::size_t> popUpCandidateIndices(const BattedBall& ball) {
    const double spray = ball.sprayAngle;
    if (spray <= -20.0)
        return {indexForPosition(FieldPosition::ThirdBase),
                indexForPosition(FieldPosition::Shortstop),
                indexForPosition(FieldPosition::Catcher)};
    if (spray >= 20.0)
        return {indexForPosition(FieldPosition::FirstBase),
                indexForPosition(FieldPosition::SecondBase),
                indexForPosition(FieldPosition::Catcher)};
    return {indexForPosition(FieldPosition::Pitcher),
            indexForPosition(FieldPosition::SecondBase),
            indexForPosition(FieldPosition::Shortstop),
            indexForPosition(FieldPosition::Catcher)};
}

// Outfield fly: narrow by spray to reduce unnecessary travelTime comparisons
std::vector<std::size_t> flyCandidateIndices(const BattedBall& ball) {
    const double spray = ball.sprayAngle;
    if (spray <= -20.0)
        return {indexForPosition(FieldPosition::LeftField),
                indexForPosition(FieldPosition::CenterField)};
    if (spray >= 20.0)
        return {indexForPosition(FieldPosition::RightField),
                indexForPosition(FieldPosition::CenterField)};
    return {indexForPosition(FieldPosition::CenterField),
            indexForPosition(FieldPosition::LeftField),
            indexForPosition(FieldPosition::RightField)};
}

std::vector<std::size_t> foulFlyCandidateIndices(const BattedBall& ball) {
    const Vector3& p = ball.landingPoint;
    const bool leftSide = p.x < 0.0;

        if (p.y < 45.0) {
        return leftSide
            ? std::vector<std::size_t>{indexForPosition(FieldPosition::Catcher),
                                       indexForPosition(FieldPosition::ThirdBase),
                                       indexForPosition(FieldPosition::LeftField),
                                       indexForPosition(FieldPosition::Pitcher)}
            : std::vector<std::size_t>{indexForPosition(FieldPosition::Catcher),
                                       indexForPosition(FieldPosition::FirstBase),
                                       indexForPosition(FieldPosition::RightField),
                                       indexForPosition(FieldPosition::Pitcher)};
    }

    if (p.y < 150.0) {
        return leftSide
            ? std::vector<std::size_t>{indexForPosition(FieldPosition::ThirdBase),
                                       indexForPosition(FieldPosition::Shortstop),
                                       indexForPosition(FieldPosition::LeftField)}
            : std::vector<std::size_t>{indexForPosition(FieldPosition::FirstBase),
                                       indexForPosition(FieldPosition::SecondBase),
                                       indexForPosition(FieldPosition::RightField)};
    }

    return leftSide
        ? std::vector<std::size_t>{indexForPosition(FieldPosition::LeftField),
                                   indexForPosition(FieldPosition::ThirdBase)}
        : std::vector<std::size_t>{indexForPosition(FieldPosition::RightField),
                                   indexForPosition(FieldPosition::FirstBase)};
}

bool isPlayableFoulTerritory(const BattedBall& ball) {
    const Vector3& p = ball.landingPoint;
    const double foulWidth = std::abs(p.x) - p.y;
    return foulWidth > 0.0
        && foulWidth <= 130.0
        && p.y >= 0.0
        && p.y <= 330.0
        && std::abs(p.x) <= 360.0;
}

// ── Fielder evaluation ───────────────────────────────────────────────────

struct FieldingAttempt {
    int fielderId    = -1;
    std::string fielderName;
    Vector3 fieldingPoint;
    double travelTime    = 0.0;
    double availableTime = 0.0;
    double fielding      = 0.0;
};

bool isInfieldGroundPoint(const Vector3& point) {
    return point.y <= 155.0 && radialDistance(point) <= 175.0;
}

Vector3 groundFieldingTarget(const BattedBall& ball) {
    if (ball.bounceTrajectory.empty()) {
        return ball.finalRestPoint;
    }

    Vector3 target = ball.bounceTrajectory.front();
    for (const Vector3& point : ball.bounceTrajectory) {
        if (!isInfieldGroundPoint(point)) {
            break;
        }
        target = point;
    }
    return target;
}

FieldingAttempt evaluateFielding(const DefenseAlignment& defense,
                                 const std::vector<std::size_t>& candidates,
                                 const Vector3& target,
                                 double availableTime) {
    FieldingAttempt best;
    best.availableTime = availableTime;
    double bestTravel  = std::numeric_limits<double>::max();

    for (std::size_t i : candidates) {
        if (i >= defense.fielders.size()) continue;
        const Fielder& f  = defense.fielders[i];
        const double targetDepth = target.y;
        const double targetRadius = radialDistance(target);
        switch (f.position) {
            case FieldPosition::Pitcher:
                if (targetDepth > 78.0 || targetRadius > 88.0) continue;
                break;
            case FieldPosition::Catcher:
                if (targetDepth > 55.0 || targetRadius > 82.0) continue;
                break;
            case FieldPosition::FirstBase:
            case FieldPosition::SecondBase:
            case FieldPosition::ThirdBase:
            case FieldPosition::Shortstop:
                if (targetDepth > 175.0 || targetRadius > 205.0) continue;
                break;
            case FieldPosition::LeftField:
            case FieldPosition::CenterField:
            case FieldPosition::RightField:
                if (targetDepth < 90.0 && targetRadius < 115.0) continue;
                break;
        }
        const double eff  = std::max(0.35, f.routeEfficiency);
        const double spd  = std::max(1.0,  f.speedFeetPerSecond);
        const double t    = f.reactionSeconds + distance2d(f.startPosition, target) / (spd * eff);
        if (t < bestTravel) {
            bestTravel       = t;
            best.fielderId   = f.id;
            best.fielderName = f.name;
            best.fieldingPoint = target;
            best.travelTime  = t;
            best.fielding    = f.fielding;
        }
    }
    return best;
}

// ── Difficulty ───────────────────────────────────────────────────────────
// [0, 1] — drives how much fielding quality matters in catch probability

double grounderDifficulty(const BattedBall& ball) {
    // High EV → harder spin, faster approach, trickier hops
    double d = std::clamp((ball.exitVelocity - 80.0) / 35.0, 0.0, 1.0) * 0.55 + 0.08;
    return std::clamp(d, 0.0, 1.0);
}

double flyDifficulty(const BattedBall& ball) {
    double d = 0.0;
    d += std::clamp((ball.exitVelocity - 80.0) / 35.0, 0.0, 1.0) * 0.30;
    // Line drive (5-22°): hardest to read and time
    if      (ball.launchAngle >= 5.0  && ball.launchAngle < 22.0) d += 0.30;
    else if (ball.launchAngle >= 22.0 && ball.launchAngle < 32.0) d += 0.08;
    else if (ball.launchAngle >= 60.0)                            d -= 0.08;  // pop-up: easy
    // Short hang → little adjustment time
    d += std::clamp((2.5 - ball.hangTime) / 2.5, 0.0, 1.0) * 0.20;
    return std::clamp(d, 0.0, 1.0);
}

// ── Catch probability given fielder reached ──────────────────────────────
// Low fielding + high difficulty → more misplays/errors
// k=1.5: calibrated to produce ~3-6% error rate on typical plays

double catchProb(double fielding, double difficulty, double k = 1.5) {
    return std::clamp(1.0 - (1.0 - fielding) * difficulty * k, 0.15, 0.995);
}

// ── Resolution builder ───────────────────────────────────────────────────

PlayResolution withFielding(PlayOutcomeType type,
                            FieldingOutcomeType fieldingOutcome,
                            int outs,
                            int bases,
                            std::string description,
                            const FieldingAttempt& attempt,
                            bool madePlay) {
    PlayResolution r;
    r.type                  = type;
    r.fieldingOutcome       = fieldingOutcome;
    r.outsRecorded          = outs;
    r.basesAwarded          = bases;
    r.fielderId             = attempt.fielderId;
    r.fielderName           = attempt.fielderName;
    r.madeFieldingPlay      = madePlay;
    r.fieldingPoint         = attempt.fieldingPoint;
    r.fielderTravelTime     = attempt.travelTime;
    r.fieldingAvailableTime = attempt.availableTime;
    r.description           = std::move(description);
    return r;
}

PlayResolution hitResolution(const BattedBall& ball, const FieldingAttempt& attempt) {
    const double spray = std::abs(ball.sprayAngle);

    if (ball.fenceCrossHeight > 0.0)
        return withFielding(PlayOutcomeType::Double, FieldingOutcomeType::NotFielded,
                            0, 2, "off the wall", attempt, false);
    if (spray >= 25.0 && ball.estimatedDistance >= 295.0)
        return withFielding(PlayOutcomeType::Triple, FieldingOutcomeType::NotFielded,
                            0, 3, "into the deep gap", attempt, false);
    if (ball.estimatedDistance >= 355.0)
        return withFielding(PlayOutcomeType::Double, FieldingOutcomeType::NotFielded,
                            0, 2, "extra-base hit", attempt, false);
    if (spray > 8.0 && ball.estimatedDistance >= 285.0)
        return withFielding(PlayOutcomeType::Double, FieldingOutcomeType::NotFielded,
                            0, 2, "into the gap", attempt, false);
    if (ball.launchAngle < 5.0 || ball.classification == "ground ball"
        || ball.classification == "hard ground ball") {
        if (ball.estimatedDistance >= 210.0)
            return withFielding(PlayOutcomeType::Double, FieldingOutcomeType::NotFielded,
                                0, 2, "ground ball into the gap", attempt, false);
        return withFielding(PlayOutcomeType::Single, FieldingOutcomeType::NotFielded,
                            0, 1, "ground ball single", attempt, false);
    }
    return withFielding(PlayOutcomeType::Single, FieldingOutcomeType::NotFielded,
                        0, 1, "base hit", attempt, false);
}

} // namespace

// ── Standard defense setup ────────────────────────────────────────────────
// Speeds are in ft/s (= meters(m/s) converts m/s → ft/s).
// Real MLB fielders sprint at ~28 ft/s; average coverage speed ~20-26 ft/s.
// IF: 18-20 ft/s, OF: 22-26 ft/s.
DefenseAlignment DefenseAlignment::standard() {
    DefenseAlignment defense;

    //                id  name  position                    startPos (ft)         spd(fps)  react  field  arm(ft) routeEff
    defense.fielders[0] = {0, "P",  FieldPosition::Pitcher,
                           {0.0, meters(18.4), 0.0},          meters(5.0), 0.35, 0.70, meters(35.0), 0.88};
    defense.fielders[1] = {1, "C",  FieldPosition::Catcher,
                           {0.0, meters(1.5),  0.0},          meters(4.5), 0.25, 0.80, meters(45.0), 0.85};
    defense.fielders[2] = {2, "1B", FieldPosition::FirstBase,
                           {meters(16.0), meters(21.0), 0.0}, meters(5.5), 0.32, 0.86, meters(40.0), 0.90};
    defense.fielders[3] = {3, "2B", FieldPosition::SecondBase,
                           {meters(12.0), meters(31.0), 0.0}, meters(6.2), 0.28, 0.84, meters(42.0), 0.93};
    defense.fielders[4] = {4, "3B", FieldPosition::ThirdBase,
                           {-meters(16.0), meters(21.0), 0.0},meters(5.8), 0.32, 0.83, meters(45.0), 0.90};
    defense.fielders[5] = {5, "SS", FieldPosition::Shortstop,
                           {-meters(12.0), meters(31.0), 0.0},meters(6.5), 0.28, 0.85, meters(45.0), 0.93};
    defense.fielders[6] = {6, "LF", FieldPosition::LeftField,
                           {-meters(42.0), meters(65.0), 0.0},meters(6.4), 0.48, 0.80, meters(38.0), 0.87};
    defense.fielders[7] = {7, "CF", FieldPosition::CenterField,
                           {0.0, meters(82.0), 0.0},          meters(7.2), 0.43, 0.86, meters(42.0), 0.92};
    defense.fielders[8] = {8, "RF", FieldPosition::RightField,
                           {meters(42.0), meters(65.0), 0.0}, meters(6.4), 0.48, 0.80, meters(42.0), 0.87};

    return defense;
}

// ──────────────────────────────────────────────────────────────────────────
// Defense AI — resolve() main entry
//
// Ground balls: zone heuristic for reach + probabilistic catch/error
// Pop-ups:      IF/C coverage + near-certain catch
// Fly balls:    physics-based reach (time sigmoid) + probabilistic catch
// ──────────────────────────────────────────────────────────────────────────
PlayResolution PlayResolutionEngine::resolve(const BattedBall& ball,
                                             const GameState& state,
                                             Random& random,
                                             const DefenseAlignment& defense,
                                             const BallparkConfig& ballpark) const {
    const DefenseAlignment standard = DefenseAlignment::standard();
    const DefenseAlignment& active  = defense.fielders[0].id < 0 ? standard : defense;

    // ── Bunt: always a short grounder, handled before normal ball-in-play logic ──
    if (ball.isBunt) {
        const std::vector<std::size_t> buntCandidates = {
            indexForPosition(FieldPosition::Pitcher),
            indexForPosition(FieldPosition::Catcher),
            indexForPosition(FieldPosition::FirstBase),
            indexForPosition(FieldPosition::ThirdBase)
        };
        const Vector3 buntTarget = ball.finalRestPoint;
        const double buntAvailableTime = ball.hangTime
            + static_cast<double>(ball.bounceTrajectory.size()) * TimeStep;

        FieldingAttempt attempt = evaluateFielding(active, buntCandidates, buntTarget, buntAvailableTime);

        // ── バント結果分岐 ──────────────────────────────────────────────────
        // 25%: バント安打
        if (attempt.fielderId < 0 || random.real(0.0, 1.0) >= 0.75) {
            PlayResolution r;
            r.type              = PlayOutcomeType::Single;
            r.fieldingOutcome   = FieldingOutcomeType::NotFielded;
            r.basesAwarded      = 1;
            r.fielderId         = attempt.fielderId;
            r.fielderName       = attempt.fielderName;
            r.fieldingPoint     = buntTarget;
            r.fielderTravelTime = attempt.travelTime;
            r.fieldingAvailableTime = buntAvailableTime;
            r.description       = "bunt single";
            return r;
        }

        // 守備が処理した場合: FC 機会を確認
        // r1B のみ・2アウト未満 → 30%確率でバントFC (先の走者を狙う)
        const bool r1B    = state.bases[0].has_value();
        const bool r2B    = state.bases[1].has_value();
        const bool canBuntFC = r1B && !r2B && state.outs < 2;
        if (canBuntFC && random.real(0.0, 1.0) < 0.30) {
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::FieldedCleanly,
                                1, 0, "bunt fc", attempt, true);
        }

        // 標準: 犠打アウト
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::FieldedCleanly,
                            1, 0, "bunt", attempt, true);
    }

    if (!ball.landsFair) {
        const bool catchableFoulFly = !isGroundBall(ball)
                                      && ball.hangTime >= 1.0
                                      && ball.maxHeight >= 10.0
                                      && isPlayableFoulTerritory(ball);
        if (catchableFoulFly) {
            const auto candidates = foulFlyCandidateIndices(ball);
            FieldingAttempt attempt = evaluateFielding(active, candidates,
                                                       ball.landingPoint,
                                                       ball.hangTime);
            if (attempt.fielderId >= 0) {
                return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                    1, 0, "foul fly out", attempt, true);
            }
        }
        return {PlayOutcomeType::Foul, FieldingOutcomeType::NotApplicable,
                0, 0, -1, "", false, {}, 0.0, 0.0, "foul ball"};
    }
    if (ball.crossesFence && ball.fenceCrossHeight >= ballpark.fenceHeightFeet) {
        return {PlayOutcomeType::HomeRun, FieldingOutcomeType::NotApplicable,
                0, 4, -1, "", false, {}, 0.0, 0.0, "home run"};
    }

    const bool groundBall = isGroundBall(ball);
    const Vector3 target  = groundBall && ball.finalRestPoint.y > 0.0
                            ? groundFieldingTarget(ball) : ball.landingPoint;
    const double groundTime = ball.hangTime
                              + static_cast<double>(ball.bounceTrajectory.size()) * TimeStep;
    const double availableTime = groundBall
                                 ? std::max(ball.hangTime, groundTime)
                                 : ball.hangTime;

    // ── 1. Ground ball ─────────────────────────────────────────────────────
    if (groundBall) {
        const auto candidates = groundCandidateIndices(ball);
        FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);

        if (attempt.fielderId < 0) return hitResolution(ball, attempt);

        // Ball must be in a "playable zone" for any fielder
        const bool inZone =
            ball.exitVelocity < 112.0
            && (ball.estimatedDistance < 134.0 || radialDistance(target) < 165.0);
        if (!inZone) return hitResolution(ball, attempt);

        // P(reach): high EV grounders blow past fielder even in zone
        // EV ≤ 82 → reach ≈ 1.0, EV = 112 → reach ≈ 0.15
        const double evReach = std::clamp((112.0 - ball.exitVelocity) / 18.0 + 0.15, 0.15, 1.0);
        if (random.real(0.0, 1.0) >= evReach) return hitResolution(ball, attempt);

        // P(catch | reached)
        const double diff = grounderDifficulty(ball);
        const double cp   = catchProb(attempt.fielding, diff);
        if (random.real(0.0, 1.0) < cp) {
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::FieldedCleanly,
                                1, 0, "ground out", attempt, true);
        }
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Error,
                            0, 0, "error", attempt, false);
    }

    // ── 2a. Infield Fly Rule (launchAngle 25–90°, ≤195 ft, r1B+r2B, <2 outs) ─
    // Rule applies regardless of whether the ball is actually caught.
    // Must be checked before isPopUp so 25–50° range is covered.
    const bool iffCondition = ball.launchAngle >= 25.0
        && ball.estimatedDistance <= 195.0
        && state.outs < 2
        && state.bases[0].has_value()
        && state.bases[1].has_value();

    if (iffCondition) {
        const auto ifCandidates = popUpCandidateIndices(ball);
        FieldingAttempt ifAttempt = evaluateFielding(active, ifCandidates, target, availableTime);
        // Even if no infielder reached (very rare), batter is still automatically out
        PlayResolution r;
        if (ifAttempt.fielderId >= 0) {
            r = withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                             1, 0, "infield fly", ifAttempt, true);
        } else {
            r.type           = PlayOutcomeType::Out;
            r.fieldingOutcome = FieldingOutcomeType::Caught;
            r.outsRecorded   = 1;
            r.description    = "infield fly";
        }
        r.infieldFly = true;
        return r;
    }

    // ── 2b. Pop-up (high LA ≥50°, short distance; IFF already handled above) ─
    const bool isPopUp = ball.launchAngle >= 50.0 && ball.estimatedDistance <= 190.0;
    if (isPopUp) {
        const auto candidates = popUpCandidateIndices(ball);
        FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);

        if (attempt.fielderId < 0) {
            const auto ofCandidates = flyCandidateIndices(ball);
            FieldingAttempt ofAttempt = evaluateFielding(active, ofCandidates, target, availableTime);
            if (ofAttempt.fielderId < 0) return hitResolution(ball, ofAttempt);
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                1, 0, "pop fly out", ofAttempt, true);
        }

        const double diff = std::clamp(flyDifficulty(ball) * 0.4, 0.0, 0.4);
        const double cp   = catchProb(attempt.fielding, diff);
        if (random.real(0.0, 1.0) < cp) {
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                1, 0, "pop fly out", attempt, true);
        }
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Error,
                            0, 0, "error on pop-up", attempt, false);
    }

    // ── 3. Outfield fly ────────────────────────────────────────────────────
    const auto candidates  = flyCandidateIndices(ball);
    FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);

    if (attempt.fielderId < 0) return hitResolution(ball, attempt);

    // Time-based reach probability
    // Negative margin → ball falls in; positive margin → likely caught
    const double margin    = availableTime - attempt.travelTime;
    const double reachProb = 1.0 / (1.0 + std::exp(-10.0 * margin));
    if (random.real(0.0, 1.0) >= reachProb) return hitResolution(ball, attempt);

    // P(catch | reached): line drives and hard hits stress even good OF
    const double diff = flyDifficulty(ball);
    const double cp   = catchProb(attempt.fielding, diff);
    if (random.real(0.0, 1.0) < cp) {
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                            1, 0, "fly out", attempt, true);
    }
    return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Error,
                        0, 0, "error", attempt, false);
}

bool PlayResolutionEngine::isGroundBall(const BattedBall& ball) {
    return ball.launchAngle < 5.0
        || ball.classification == "ground ball"
        || ball.classification == "hard ground ball";
}

std::string toString(PlayOutcomeType type) {
    switch (type) {
        case PlayOutcomeType::Foul:    return "foul";
        case PlayOutcomeType::Out:     return "out";
        case PlayOutcomeType::Single:  return "single";
        case PlayOutcomeType::Double:  return "double";
        case PlayOutcomeType::Triple:  return "triple";
        case PlayOutcomeType::HomeRun: return "home run";
    }
    return "unknown";
}

} // namespace joji
