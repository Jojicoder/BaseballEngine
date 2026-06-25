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
    const double spray    = ball.sprayAngle;
    const double landDist = radialDistance(ball.landingPoint);
    // Use roll-out position for candidate selection: balls landing at 10-30 ft
    // (very negative LA) roll 60-140 ft into IF territory. Without this, only
    // Catcher/Pitcher are selected — both fail the depth filter → auto-hit.
    const double restDist = radialDistance(ball.finalRestPoint);
    const double d        = std::max(landDist, std::min(restDist, 174.0));

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
    FieldPosition position = FieldPosition::Pitcher;
    Vector3 fieldingPoint;
    double travelTime    = 0.0;
    double availableTime = 0.0;
    double fielding      = 0.0;
};

struct FieldingTarget {
    Vector3 point;
    double availableTime = 0.0;
};

bool isInfieldGroundPoint(const Vector3& point) {
    return point.y <= 155.0 && radialDistance(point) <= 175.0;
}

bool hasFenceIntersection(const BattedBall& ball) {
    return std::abs(ball.fenceIntersectionPoint.x) > 0.001
        || std::abs(ball.fenceIntersectionPoint.y) > 0.001
        || std::abs(ball.fenceIntersectionPoint.z) > 0.001;
}

FieldingTarget groundFieldingTarget(const BattedBall& ball) {
    FieldingTarget target;
    if (ball.bounceTrajectory.empty()) {
        target.point = ball.finalRestPoint;
        target.availableTime = ball.hangTime;
        return target;
    }

    const double absSpray = std::abs(ball.sprayAngle);
    const double desiredDepth =
        absSpray >= 28.0 ? 78.0 :
        absSpray >= 12.0 ? 98.0 : 112.0;
    target.point = ball.bounceTrajectory.front();
    std::size_t targetIndex = 0;
    for (std::size_t i = 0; i < ball.bounceTrajectory.size(); ++i) {
        const Vector3& point = ball.bounceTrajectory[i];
        if (!isInfieldGroundPoint(point)) {
            break;
        }
        target.point = point;
        targetIndex = i;
        if (point.y >= desiredDepth) {
            break;
        }
    }
    // If the trajectory stopped short of desiredDepth and finalRestPoint is
    // deeper in the infield, use it — this covers soft-negative-LA grounders
    // whose bounceTrajectory may end before the ball finishes decelerating.
    if (target.point.y < desiredDepth && isInfieldGroundPoint(ball.finalRestPoint)
        && ball.finalRestPoint.y > target.point.y) {
        target.point = ball.finalRestPoint;
        targetIndex  = ball.bounceTrajectory.size();
    }
    target.availableTime = ball.hangTime + static_cast<double>(targetIndex) * TimeStep;
    return target;
}

FieldingTarget outfieldGroundTarget(const BattedBall& ball) {
    FieldingTarget target;
    target.point = ball.finalRestPoint;
    target.availableTime = ball.hangTime
        + static_cast<double>(ball.bounceTrajectory.size()) * TimeStep;

    if (ball.fenceCrossHeight > 0.0 && !ball.crossesFence && hasFenceIntersection(ball)) {
        target.point = ball.fenceIntersectionPoint;
        target.point.z = 0.0;
        target.availableTime = std::min(target.availableTime, ball.hangTime + 0.55);
    }
    return target;
}

bool groundBallReachedOutfield(const BattedBall& ball) {
    return ball.estimatedDistance >= 180.0
        || ball.finalRestPoint.y >= 190.0
        || radialDistance(ball.finalRestPoint) >= 210.0
        || ball.fenceCrossHeight > 0.0;
}

bool arrivesInTime(const FieldingAttempt& attempt, double slackSeconds) {
    return attempt.fielderId >= 0
        && attempt.travelTime <= attempt.availableTime + slackSeconds;
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
                if (targetDepth > 195.0 || targetRadius > 220.0) continue;
                break;
            case FieldPosition::LeftField:
            case FieldPosition::CenterField:
            case FieldPosition::RightField:
                if (targetDepth < 90.0 && targetRadius < 115.0) continue;
                break;
        }
        const double eff  = std::max(0.35, f.routeEfficiency);
        const double spd  = std::max(1.0,  f.speedFeetPerSecond);
        const double acc  = std::max(1.0,  f.accelerationFeetPerSecond2);
        const double dist = distance2d(f.startPosition, target) / eff;
        // Kinematic ramp: accelerate to top speed, then cruise.
        const double tRamp = spd / acc;
        const double dRamp = 0.5 * spd * tRamp;
        const double tMove = dist <= dRamp
            ? std::sqrt(2.0 * dist / acc)
            : tRamp + (dist - dRamp) / spd;

        // Change-of-direction penalty: fielders pay extra time when the ball requires
        // them to reverse or sharply redirect their initial momentum burst.
        double codPenalty = 0.0;
        const bool isOutfielder = (f.position == FieldPosition::LeftField
                                || f.position == FieldPosition::CenterField
                                || f.position == FieldPosition::RightField);
        if (dist > 8.0) {
            const double dx = target.x - f.startPosition.x;
            const double dy = target.y - f.startPosition.y;
            const double dLen = std::sqrt(dx*dx + dy*dy);
            if (dLen > 0.1) {
                const double dxNorm = dx / dLen;
                const double dyNorm = dy / dLen;
                const double primaryY = isOutfielder ? 1.0 : -1.0;
                const double dot = dyNorm * primaryY;

                if (isOutfielder && dot > 0.25) {
                    // Over-the-head drop-step: diagonal routes are harder than pure back
                    const double lateralBonus = std::abs(dxNorm) * 0.15;
                    codPenalty = std::clamp(0.10 + (dot - 0.25) * 0.50 + lateralBonus, 0.10, 0.44);
                } else if (dot < -0.2) {
                    // Ball in opposite direction of initial read
                    codPenalty = std::clamp(0.05 + (-dot - 0.2) * 0.25, 0.05, 0.20);
                }

                // Infielder lateral: ranging to arm/cross-body side is harder.
                // SS/3B arm side = catcher's right (+x); 2B/1B arm side = catcher's left (-x).
                if (!isOutfielder && dist > 6.0) {
                    const bool leftSideIF = (f.position == FieldPosition::Shortstop
                                          || f.position == FieldPosition::ThirdBase);
                    const double armDot = dxNorm * (leftSideIF ? 1.0 : -1.0);
                    if (armDot > 0.50) {
                        codPenalty += std::clamp(0.04 + (armDot - 0.50) * 0.14, 0.04, 0.10);
                    }
                }
            }
        }
        // Fly read delay: outfielders with poor fielding misread the initial ball
        // direction and take a beat before committing to the correct route.
        const double t    = f.reactionSeconds + tMove + codPenalty
                          + (isOutfielder ? f.flyReadDelay : 0.0);
        if (t < bestTravel) {
            bestTravel         = t;
            best.fielderId     = f.id;
            best.fielderName   = f.name;
            best.position      = f.position;
            best.fieldingPoint = target;
            best.travelTime    = t;
            best.fielding      = f.fielding;
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

// Zone-based ground ball out probability — calibrated to MLB average GB out rate ~74%.
// rollSpeed: ball speed at start of rolling phase (ft/s); batterSpeed: normalized batter speed.
double groundBallOutProb(const BattedBall& ball, const FieldingAttempt& attempt,
                         double rollSpeed = 0.0, double batterSpeedNorm = 0.0) {
    const double absSpray = std::abs(ball.sprayAngle);
    double base;
    if (absSpray < 8.0)        base = 0.85; // up the middle
    else if (absSpray < 16.0)  base = 0.79; // SS/2B primary zone
    else if (absSpray < 24.0)  base = 0.71; // SS/2B edge / 1B/3B primary zone
    else if (absSpray < 33.0)  base = 0.60; // corner IF zone
    else                        base = 0.45; // extreme pull: down the line
    // Hard-hit penalty: EV 82→110 reduces out rate by up to 0.30
    const double evPenalty  = std::clamp((ball.exitVelocity - 82.0) / 28.0 * 0.30, 0.0, 0.30);
    // Slow roller bonus: roll speed < 30 ft/s → fielder has more time but tricky hops.
    // Very slow (< 15 ft/s) is hardest to field fast enough for out, but batter speed matters.
    const double slowBonus  = rollSpeed > 0.0
        ? std::clamp((30.0 - rollSpeed) / 30.0 * 0.12, 0.0, 0.12) : 0.0;
    // Fast batter penalty on slow rollers: speed bumps infield hit chance.
    const double speedPenalty = rollSpeed > 0.0 && rollSpeed < 25.0
        ? std::clamp(batterSpeedNorm * (25.0 - rollSpeed) / 25.0 * 0.14, 0.0, 0.14) : 0.0;
    // Fielder quality: 0.83 = average MLB IF; ±0.10 fielding → ±3% out rate
    const double fieldBonus = (attempt.fielding - 0.83) * 0.30;
    return std::clamp(base - evPenalty + slowBonus - speedPenalty + fieldBonus, 0.10, 0.95);
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
// k scales how much difficulty matters for a given position type.
// OF: k=1.0 (fewer errors, more "not reached" decisions already handled)
// IF corner (1B/3B): k=1.2 (react to hard shots, charging plays)
// IF middle (SS/2B): k=0.9 (elite fielders, higher fielding ratings)
// P/C: k=1.4 (tough angles, limited range)

double catchProbForPosition(double fielding, double difficulty, FieldPosition pos) {
    double k;
    switch (pos) {
        case FieldPosition::Shortstop:
        case FieldPosition::SecondBase:    k = 0.9; break;
        case FieldPosition::FirstBase:
        case FieldPosition::ThirdBase:     k = 1.2; break;
        case FieldPosition::LeftField:
        case FieldPosition::CenterField:
        case FieldPosition::RightField:    k = 1.0; break;
        default:                           k = 1.4; break; // P, C
    }
    return std::clamp(1.0 - (1.0 - fielding) * difficulty * k, 0.15, 0.995);
}

double catchProb(double fielding, double difficulty, double k = 1.1) {
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

// ── Defensive shift ───────────────────────────────────────────────────────
// Shifts infield/outfield based on batter.pullTendency (50=neutral, high=pull heavy).
// RHB pull = toward left (x < 0); LHB pull = toward right (x > 0).
// Shift sign: +1 for LHB pull direction (rightward), -1 for RHB pull direction (leftward).
DefenseAlignment applyDefensiveShift(DefenseAlignment a, const Player& batter) {
    const double pull = (batter.pullTendency - 50) / 50.0;   // -1..+1
    if (std::abs(pull) < 0.08) return a;                      // neutral: no shift

    // For RHB the pull side is left field (negative x); for LHB it is right (positive x).
    const double pullSign = (batter.battingSide == BattingSide::Right) ? -1.0 : 1.0;
    const double shiftMag = pull * pullSign;   // + = shift toward RF side, - = toward LF side

    // Infield: SS and 2B slide toward the pull side; 3B/1B stay near the line.
    // Mild shift (<0.3): SS shades 1-2 steps, 2B shades similarly
    // Strong shift (>0.5): extreme shift — both SS and 2B move to pull side of 2B bag
    const double ifShiftFt = shiftMag * 18.0;  // max 18ft lateral shift for extreme pull
    const double ofShiftFt = shiftMag * 22.0;  // OF shifts more aggressively

    auto& f = a.fielders;
    // Infield: 2B (index 3) and SS (index 5) shift laterally
    f[3].startPosition.x += ifShiftFt * 0.6;    // 2B
    f[5].startPosition.x += ifShiftFt * 0.6;    // SS
    // 1B and 3B hug lines on extreme shifts
    if (std::abs(pull) > 0.5) {
        f[2].startPosition.x += ifShiftFt * 0.2; // 1B shade toward pull side line
        f[4].startPosition.x += ifShiftFt * 0.2; // 3B shade
    }
    // Outfield: LF and RF shift toward pull side; CF plays toward pull alley
    f[6].startPosition.x += ofShiftFt * 0.5;    // LF
    f[7].startPosition.x += ofShiftFt * 0.25;   // CF
    f[8].startPosition.x += ofShiftFt * 0.5;    // RF

    return a;
}

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
            if (arrivesInTime(attempt, 0.35)) {
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
    const FieldingTarget groundTarget = groundBall
        ? groundFieldingTarget(ball)
        : FieldingTarget{ball.landingPoint, ball.hangTime};
    const Vector3 target = groundBall ? groundTarget.point : ball.landingPoint;
    const double availableTime = groundBall ? groundTarget.availableTime : ball.hangTime;

    // ── 1. Ground ball ─────────────────────────────────────────────────────
    if (groundBall) {
        const auto candidates = groundCandidateIndices(ball);
        FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);
        auto missedGroundBallResolution = [&]() {
            if (groundBallReachedOutfield(ball)) {
                const FieldingTarget ofTarget = outfieldGroundTarget(ball);
                FieldingAttempt ofAttempt = evaluateFielding(active,
                                                             flyCandidateIndices(ball),
                                                             ofTarget.point,
                                                             ofTarget.availableTime);
                if (ofAttempt.fielderId >= 0) {
                    return hitResolution(ball, ofAttempt);
                }
            }
            return hitResolution(ball, attempt);
        };

        if (attempt.fielderId < 0) return missedGroundBallResolution();

        // Ball must be in a "playable zone" for any fielder
        const bool inZone =
            ball.exitVelocity < 112.0
            && target.y <= 180.0
            && (ball.estimatedDistance < 160.0 || radialDistance(target) < 186.0);
        if (!inZone) return missedGroundBallResolution();

        if (random.real(0.0, 1.0) >= groundBallOutProb(ball, attempt,
                                                         ball.initialRollSpeed,
                                                         ball.batterSpeedNorm)) {
            return missedGroundBallResolution();
        }

        // P(catch | reached): position-specific error rate
        const double diff = grounderDifficulty(ball);
        const double cp   = catchProbForPosition(attempt.fielding, diff, attempt.position);
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
    const auto iffCandidates = popUpCandidateIndices(ball);
    FieldingAttempt iffAttempt = evaluateFielding(active, iffCandidates, target, availableTime);
    const bool ordinaryEffort = iffAttempt.fielderId >= 0
        && iffAttempt.travelTime <= availableTime + 0.45;
    const bool iffCondition = ball.launchAngle >= 25.0
        && ball.estimatedDistance <= 185.0
        && state.outs < 2
        && state.bases[0].has_value()
        && state.bases[1].has_value()
        && ordinaryEffort;

    if (iffCondition) {
        // Even if no infielder reached (very rare), batter is still automatically out
        PlayResolution r;
        r = withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                         1, 0, "infield fly", iffAttempt, true);
        r.infieldFly = true;
        return r;
    }

    // ── 2b. Pop-up (high LA ≥50°, short distance; IFF already handled above) ─
    const bool isPopUp = ball.launchAngle >= 50.0 && ball.estimatedDistance <= 190.0;
    if (isPopUp) {
        const auto candidates = popUpCandidateIndices(ball);
        FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);

        if (!arrivesInTime(attempt, 0.45)) {
            const auto ofCandidates = flyCandidateIndices(ball);
            FieldingAttempt ofAttempt = evaluateFielding(active, ofCandidates, target, availableTime);
            if (!arrivesInTime(ofAttempt, 0.35)) return hitResolution(ball, ofAttempt);
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                1, 0, "pop fly out", ofAttempt, true);
        }

        const double diff = std::clamp(flyDifficulty(ball) * 0.4, 0.0, 0.4);
        const double cp   = catchProbForPosition(attempt.fielding, diff, attempt.position);
        if (random.real(0.0, 1.0) < cp) {
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                1, 0, "pop fly out", attempt, true);
        }
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Error,
                            0, 0, "error on pop-up", attempt, false);
    }

    // ── 2c. Bloop / shallow liner / shallow fly ───────────────────────────────
    // LA 10-31°, estimatedDistance 100-252ft: IF/OF gap territory.
    // Neither kinematic model can accurately place IF sprinting out 80-150ft
    // or OF charging in 20-80ft in 1.5-3.5s. Direct probability model instead.
    // Hang time is the primary physical driver (longer = more time to read and run).
    // Extended from (218ft, EV<94) to (252ft, EV<102) to capture hard LDs that
    // previously fell through to OF fly path → auto-hit (no-man's land).
    const bool isBloop = ball.launchAngle >= 10.0 && ball.launchAngle < 31.0
                         && ball.estimatedDistance >= 100.0
                         && ball.estimatedDistance < 252.0
                         && ball.exitVelocity < 108.0;
    if (isBloop) {
        // SS covers pull-side (spray ≤ 0°); 2B covers oppo-side.
        // For balls 200-252ft, represents whichever IF/OF breaks hardest.
        const std::size_t miIdx = (ball.sprayAngle <= 0.0)
                                  ? indexForPosition(FieldPosition::Shortstop)
                                  : indexForPosition(FieldPosition::SecondBase);
        const Fielder& mi = active.fielders[miIdx];
        // distFactor: peak at 155ft (optimal sprint range for SS/2B), falls off to either side
        const double distFactor = std::clamp(1.0 - std::abs(ball.estimatedDistance - 155.0) / 97.0,
                                             0.0, 0.62);
        // Longer hang → fielder has more time to get a read and sprint
        const double hangBonus  = std::clamp((ball.hangTime - 1.2) / 3.2, 0.0, 0.22);
        // Hard contact → harder to read trajectory off the bat
        const double evPenalty  = std::clamp((ball.exitVelocity - 80.0) / 44.0 * 0.18, 0.0, 0.18);
        const double fieldBonus = (mi.fielding - 0.83) * 0.22;
        const double catchP     = std::clamp(distFactor + hangBonus - evPenalty + fieldBonus,
                                             0.0, 0.74);
        if (random.real(0.0, 1.0) < catchP) {
            FieldingAttempt bloopAtt;
            bloopAtt.fielderId     = mi.id;
            bloopAtt.fielderName   = mi.name;
            bloopAtt.position      = mi.position;
            bloopAtt.fielding      = mi.fielding;
            bloopAtt.travelTime    = ball.hangTime * 0.85;
            bloopAtt.availableTime = ball.hangTime;
            bloopAtt.fieldingPoint = ball.landingPoint;
            return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                1, 0, "bloop out", bloopAtt, true);
        }
    }

    // ── 3. Outfield fly ────────────────────────────────────────────────────
    const auto candidates  = flyCandidateIndices(ball);
    FieldingAttempt attempt = evaluateFielding(active, candidates, target, availableTime);

    if (attempt.fielderId < 0) return hitResolution(ball, attempt);

    // Time-based reach probability — sigmoid centered on time margin.
    // Line drives (LA 10-25°): short hang, hard read, fall in often → no sigmoid offset.
    // Normal fly balls (LA >25°): shift sigmoid toward "caught" since fielders read these well.
    const double margin = availableTime - attempt.travelTime;
    const bool isLineDrive  = ball.launchAngle >= 10.0 && ball.launchAngle < 25.0;
    // Shallow fly (LA 31-46°, dist < 200ft): OF charges in aggressively — easier than normal fly
    const bool isShallowFly = ball.launchAngle >= 31.0 && ball.launchAngle < 46.0
                              && ball.estimatedDistance < 200.0;
    const double sigOffset = isLineDrive ? 0.0 : (isShallowFly ? 0.8 : 0.4);
    const double reachProb = 1.0 / (1.0 + std::exp(-5.0 * (margin + sigOffset)));
    if (random.real(0.0, 1.0) >= reachProb) {
        // Diving catch: elite outfielders can dive for near-misses
        const double gap = attempt.travelTime - attempt.availableTime;
        if (gap > 0.0 && gap <= 0.38) {
            const double diveChance = attempt.fielding * (0.38 - gap) / 0.38 * 0.36;
            if (random.real(0.0, 1.0) < diveChance) {
                return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                                    1, 0, "diving catch", attempt, true);
            }
        }
        return hitResolution(ball, attempt);
    }

    // P(catch | reached): line drives and hard hits stress even good OF
    const double diff = flyDifficulty(ball);
    const double cp   = catchProbForPosition(attempt.fielding, diff, attempt.position);
    if (random.real(0.0, 1.0) < cp) {
        return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Caught,
                            1, 0, "fly out", attempt, true);
    }
    return withFielding(PlayOutcomeType::Out, FieldingOutcomeType::Error,
                        0, 0, "error", attempt, false);
}

bool PlayResolutionEngine::isGroundBall(const BattedBall& ball) {
    // LA < 10° includes "low liners" that stay in the infield — route through IF zone model
    return ball.launchAngle < 10.0
        || ball.classification == "ground ball"
        || ball.classification == "hard ground ball"
        || ball.classification == "low liner";
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
