#include "AnimationPlanBuilder.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace joji {

namespace {

constexpr double PitchDistanceFeet = 60.5;
constexpr double FeetPerMphSecond = 1.4666666667;
constexpr double PitcherReleaseHeightFeet = 6.0;
constexpr double DefaultBattedBallRollSpeedFeetPerSecond = 45.0;

AnimationPoint makePoint(const Vector3& point, double timeSeconds) {
    return {point.x, point.y, point.z, timeSeconds};
}

AnimationPoint lerpPoint(const AnimationPoint& start,
                         const AnimationPoint& end,
                         double amount,
                         double movementXFeet,
                         double movementZFeet) {
    const double breakAmount = amount * amount;
    return {
        start.x + (end.x - start.x) * amount + movementXFeet * breakAmount,
        start.y + (end.y - start.y) * amount,
        start.z + (end.z - start.z) * amount + movementZFeet * breakAmount,
        start.timeSeconds + (end.timeSeconds - start.timeSeconds) * amount
    };
}

double pitchDurationSeconds(const Pitch& pitch) {
    const double velocityFeetPerSecond = std::max(1.0, pitch.pitchVelocity * FeetPerMphSecond);
    return PitchDistanceFeet / velocityFeetPerSecond;
}

bool isStrikeOutcome(PitchOutcome outcome) {
    return outcome == PitchOutcome::CalledStrike
        || outcome == PitchOutcome::SwingingStrike
        || outcome == PitchOutcome::Foul
        || outcome == PitchOutcome::InPlay;
}

bool isBallOutcome(PitchOutcome outcome) {
    return outcome == PitchOutcome::Ball;
}

int zoneNumber(double locationX, double locationZ) {
    constexpr double zoneLeft = -0.83;
    constexpr double zoneRight = 0.83;
    constexpr double zoneBottom = 1.5;
    constexpr double zoneTop = 3.5;

    const double clampedX = std::clamp(locationX, zoneLeft, zoneRight);
    const double clampedZ = std::clamp(locationZ, zoneBottom, zoneTop);
    const int col = std::min(2, std::max(0, static_cast<int>(
        (clampedX - zoneLeft) / ((zoneRight - zoneLeft) / 3.0))));
    const int row = std::min(2, std::max(0, static_cast<int>(
        (zoneTop - clampedZ) / ((zoneTop - zoneBottom) / 3.0))));
    return row * 3 + col + 1;
}

BattingSide resolvedBatterSide(BattingSide batterSide, ThrowingHand pitcherHand) {
    if (batterSide != BattingSide::Switch) {
        return batterSide;
    }
    return pitcherHand == ThrowingHand::Right ? BattingSide::Left : BattingSide::Right;
}

std::string describePlan(const AtBatResult& atBat, const PlayResolution& resolution) {
    std::ostringstream out;
    out << atBat.batter.name << " vs " << atBat.pitcher.name << ": ";
    if (!resolution.description.empty()) {
        out << resolution.description;
    } else {
        out << toString(atBat.finalOutcome);
    }
    return out.str();
}

void appendTimedTrajectory(std::vector<AnimationPoint>& points,
                           const std::vector<Vector3>& trajectory,
                           double startSeconds,
                           double durationSeconds) {
    if (trajectory.empty()) {
        return;
    }

    const double denominator = trajectory.size() > 1
        ? static_cast<double>(trajectory.size() - 1)
        : 1.0;
    for (std::size_t i = 0; i < trajectory.size(); ++i) {
        const double amount = static_cast<double>(i) / denominator;
        points.push_back(makePoint(trajectory[i], startSeconds + durationSeconds * amount));
    }
}

double rollDurationSeconds(const BattedBall& ball) {
    if (ball.bounceTrajectory.empty()) {
        return 0.0;
    }
    return std::max(0.2, ball.groundRollDistance / DefaultBattedBallRollSpeedFeetPerSecond);
}

double distanceSquared2d(const Vector3& left, const Vector3& right) {
    const double dx = left.x - right.x;
    const double dy = left.y - right.y;
    return dx * dx + dy * dy;
}

double radialDistance2d(const Vector3& point) {
    return std::sqrt(point.x * point.x + point.y * point.y);
}

std::size_t closestTrajectoryIndex(const std::vector<Vector3>& trajectory, const Vector3& target) {
    std::size_t closest = 0;
    double closestDistance = 1.0e18;
    for (std::size_t i = 0; i < trajectory.size(); ++i) {
        const double distance = distanceSquared2d(trajectory[i], target);
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = i;
        }
    }
    return closest;
}

double visualFenceY(double xFeet) {
    return 405.0 - 0.0012 * xFeet * xFeet;
}

Vector3 clampPointInsideVisualFence(Vector3 point) {
    point.x = std::clamp(point.x, -295.0, 295.0);
    const double fenceY = visualFenceY(point.x);
    if (point.y > fenceY - 8.0) {
        point.y = fenceY - 8.0;
    }
    return point;
}

bool isCaughtBall(const PlayResolution& resolution) {
    return resolution.type == PlayOutcomeType::Out
        && resolution.fieldingOutcome == FieldingOutcomeType::Caught;
}

bool hasFieldingPoint(const Vector3& point) {
    return std::abs(point.x) > 0.001
        || std::abs(point.y) > 0.001
        || std::abs(point.z) > 0.001;
}

bool isFieldedGroundBall(const PlayResolution& resolution) {
    return (resolution.fieldingOutcome == FieldingOutcomeType::FieldedCleanly
            || resolution.fieldingOutcome == FieldingOutcomeType::Error)
        && hasFieldingPoint(resolution.fieldingPoint);
}

bool isGroundBallLike(const BattedBall& battedBall) {
    return battedBall.launchAngle < 5.0
        || battedBall.classification.find("ground") != std::string::npos;
}

bool isInfieldSingleVisual(const PlayResolution& resolution, const BattedBall& battedBall) {
    return resolution.type == PlayOutcomeType::Single
        && resolution.fieldingOutcome == FieldingOutcomeType::NotFielded
        && resolution.fielderId >= 0
        && hasFieldingPoint(resolution.fieldingPoint)
        && isGroundBallLike(battedBall)
        && radialDistance2d(resolution.fieldingPoint) <= 190.0
        && resolution.fieldingPoint.y <= 175.0;
}

Vector3 fieldingTargetPoint(const PlayResolution& resolution, const BattedBall& battedBall) {
    if (resolution.fielderId >= 0 && hasFieldingPoint(resolution.fieldingPoint)) {
        return resolution.fieldingPoint;
    }
    return isCaughtBall(resolution) ? battedBall.landingPoint : battedBall.finalRestPoint;
}

Vector3 throwStartPoint(const PlayResult& result) {
    if (result.fielderId >= 0 && hasFieldingPoint(result.fieldingPoint)) {
        return result.fieldingPoint;
    }
    return result.fieldingOutcome == FieldingOutcomeType::Caught
        ? result.battedBall.landingPoint
        : result.battedBall.finalRestPoint;
}

} // namespace

AnimationPlan AnimationPlanBuilder::build(const AtBatResult& atBat,
                                          const PlayResolution& resolution,
                                          const BattedBall& battedBall,
                                          const DefenseAlignment& defense,
                                          const std::string& gameId) const {
    AnimationPlan plan;
    plan.gameId = gameId;
    plan.description = describePlan(atBat, resolution);

    plan.pitch = buildPitchAnimation(atBat);
    plan.hasPitch = !plan.pitch.points.empty();

    plan.battedBall = buildBattedBallAnimation(atBat, resolution, battedBall);
    plan.hasBattedBall = !plan.battedBall.points.empty();

    plan.runners   = buildRunnerAnimations(atBat, resolution);
    plan.defenders = buildDefenseAnimations(resolution, battedBall, defense);
    // throws are empty here; rebuilt in applyPendingAtBatResult after applyPlay()

    plan.totalDurationSeconds = std::max(plan.pitch.durationSeconds,
                                         plan.battedBall.durationSeconds);
    for (const RunnerAnimation& r : plan.runners) {
        plan.totalDurationSeconds = std::max(plan.totalDurationSeconds, r.durationSeconds);
    }
    for (const DefenseAnimation& d : plan.defenders) {
        plan.totalDurationSeconds = std::max(plan.totalDurationSeconds, d.durationSeconds);
    }
    // throws duration added by applyPendingAtBatResult after rebuild

    return plan;
}

PitchAnimation AnimationPlanBuilder::buildPitchAnimation(const AtBatResult& atBat) const {
    PitchAnimation animation;
    animation.pitcherName = atBat.pitcher.name;
    animation.batterName = atBat.batter.name;

    if (atBat.pitchLogs.empty()) {
        return animation;
    }

    const PitchLog& log = atBat.pitchLogs.back();
    const Pitch& pitch = log.pitch;
    animation.pitchType = toString(pitch.pitchType);
    animation.velocity = pitch.pitchVelocity;
    animation.movementX = pitch.movementX;
    animation.movementZ = pitch.movementZ;
    animation.pitcherHand = atBat.pitcher.throwingHand;
    animation.batterSide = resolvedBatterSide(atBat.batter.battingSide, atBat.pitcher.throwingHand);
    animation.result = toString(log.pitchOutcome);
    animation.endLocationX = pitch.locationX;
    animation.endLocationZ = pitch.locationZ;
    animation.zoneNumber = zoneNumber(pitch.locationX, pitch.locationZ);
    animation.durationSeconds = pitchDurationSeconds(pitch);
    animation.isStrike = isStrikeOutcome(log.pitchOutcome);
    animation.isBall = isBallOutcome(log.pitchOutcome);
    animation.isInPlay = log.pitchOutcome == PitchOutcome::InPlay;
    animation.start = {0.0, PitchDistanceFeet, PitcherReleaseHeightFeet, 0.0};
    animation.end = {pitch.locationX, 0.0, pitch.locationZ, animation.durationSeconds};

    constexpr int Segments = 8;
    animation.points.reserve(Segments + 1);
    const double movementXFeet = pitch.movementX / 12.0;
    const double movementZFeet = pitch.movementZ / 12.0;
    for (int i = 0; i <= Segments; ++i) {
        const double amount = static_cast<double>(i) / Segments;
        animation.points.push_back(lerpPoint(animation.start,
                                             animation.end,
                                             amount,
                                             movementXFeet,
                                             movementZFeet));
    }

    return animation;
}

BattedBallAnimation AnimationPlanBuilder::buildBattedBallAnimation(
    const AtBatResult& atBat,
    const PlayResolution& resolution,
    const BattedBall& battedBall) const {
    BattedBallAnimation animation;
    animation.batterName = atBat.batter.name;
    animation.result = toString(resolution.type);
    animation.classification = battedBall.classification;
    animation.estimatedDistance = battedBall.estimatedDistance;
    animation.maxHeight = battedBall.maxHeight;
    animation.landsFair = battedBall.landsFair;
    animation.crossesFence = battedBall.crossesFence;
    animation.landingPoint = makePoint(battedBall.landingPoint, battedBall.hangTime);

    if (battedBall.trajectory.empty()) {
        animation.finalPoint = animation.landingPoint;
        return animation;
    }

    appendTimedTrajectory(animation.points,
                          battedBall.trajectory,
                          0.0,
                          battedBall.hangTime);

    if (isCaughtBall(resolution)) {
        animation.durationSeconds = battedBall.hangTime;
        animation.finalPoint = animation.landingPoint;
        if (animation.points.empty()
            || animation.points.back().x != animation.finalPoint.x
            || animation.points.back().y != animation.finalPoint.y
            || animation.points.back().z != animation.finalPoint.z) {
            animation.points.push_back(animation.finalPoint);
        }
        return animation;
    }

    if (isFieldedGroundBall(resolution) || isInfieldSingleVisual(resolution, battedBall)) {
        const Vector3 fieldingPoint = resolution.fieldingPoint;
        const double endTime = std::max(battedBall.hangTime, resolution.fielderTravelTime + 0.08);
        if (!battedBall.bounceTrajectory.empty() && endTime > battedBall.hangTime) {
            const std::size_t closestIndex = closestTrajectoryIndex(battedBall.bounceTrajectory,
                                                                    fieldingPoint);
            std::vector<Vector3> fieldedBounce;
            fieldedBounce.reserve(closestIndex + 1);
            for (std::size_t i = 0; i <= closestIndex; ++i) {
                fieldedBounce.push_back(battedBall.bounceTrajectory[i]);
            }
            appendTimedTrajectory(animation.points,
                                  fieldedBounce,
                                  battedBall.hangTime,
                                  endTime - battedBall.hangTime);
        }

        animation.durationSeconds = endTime;
        animation.finalPoint = makePoint(fieldingPoint, endTime);
        if (animation.points.empty()
            || animation.points.back().x != animation.finalPoint.x
            || animation.points.back().y != animation.finalPoint.y
            || animation.points.back().z != animation.finalPoint.z) {
            animation.points.push_back(animation.finalPoint);
        }
        return animation;
    }

    const double bounceDuration = rollDurationSeconds(battedBall);
    appendTimedTrajectory(animation.points,
                          battedBall.bounceTrajectory,
                          battedBall.hangTime,
                          bounceDuration);

    animation.durationSeconds = battedBall.hangTime + bounceDuration;
    animation.finalPoint = makePoint(battedBall.finalRestPoint,
                                     animation.durationSeconds);
    if (animation.points.empty()
        || animation.points.back().x != animation.finalPoint.x
        || animation.points.back().y != animation.finalPoint.y
        || animation.points.back().z != animation.finalPoint.z) {
        animation.points.push_back(animation.finalPoint);
    }

    return animation;
}

std::vector<RunnerAnimation> AnimationPlanBuilder::buildRunnerAnimations(
    const AtBatResult& atBat,
    const PlayResolution& resolution) const {
    (void)atBat;
    (void)resolution;
    return {};
}

std::vector<DefenseAnimation> AnimationPlanBuilder::buildDefenseAnimations(
    const PlayResolution& resolution,
    const BattedBall& battedBall,
    const DefenseAlignment& defense) const {
    if (resolution.fielderId < 0 || resolution.fielderTravelTime <= 0.0) {
        return {};
    }

    // Find the active fielder
    const Fielder* fielder = nullptr;
    for (const auto& f : defense.fielders) {
        if (f.id == resolution.fielderId) {
            fielder = &f;
            break;
        }
    }
    if (!fielder) return {};

    DefenseAnimation anim;
    anim.fielderId      = resolution.fielderId;
    anim.fielderName    = resolution.fielderName;
    anim.madePlay       = resolution.madeFieldingPlay;
    anim.durationSeconds = resolution.fielderTravelTime;

    const double sx = fielder->startPosition.x;
    const double sy = fielder->startPosition.y;
    const Vector3 target = clampPointInsideVisualFence(fieldingTargetPoint(resolution, battedBall));
    const double tx = target.x;
    const double ty = target.y;
    const double reaction = std::clamp(fielder->reactionSeconds, 0.0, anim.durationSeconds * 0.25);

    // Path: hold at start during reaction → straight line to target
    anim.points.push_back({sx, sy, 0.0, 0.0});
    if (reaction > 0.0) {
        anim.points.push_back({sx, sy, 0.0, reaction});
    }
    anim.points.push_back({tx, ty, 0.0, anim.durationSeconds});

    return {anim};
}

namespace {

struct BasePos { double x, y; };
constexpr BasePos kBases[5] = {
    {0.0,   0.0},  // [0] unused
    {90.0,  90.0}, // [1] 1B
    {0.0,  180.0}, // [2] 2B
    {-90.0, 90.0}, // [3] 3B
    {0.0,   0.0},  // [4] home
};

ThrowAnimation makeThrow(int fielderId, const std::string& fielderName, int targetBase,
                         double fromX, double fromY, double startOffset,
                         double throwSpeed) {
    const BasePos& to = kBases[targetBase];
    const double dist = std::sqrt((to.x - fromX) * (to.x - fromX) +
                                  (to.y - fromY) * (to.y - fromY));
    ThrowAnimation t;
    t.fielderId       = fielderId;
    t.fielderName     = fielderName;
    t.targetBase      = targetBase;
    t.startTimeOffset = startOffset;
    t.durationSeconds = dist / throwSpeed;
    t.points.push_back({fromX, fromY, 0.0, startOffset});
    t.points.push_back({to.x,  to.y,  0.0, startOffset + t.durationSeconds});
    return t;
}

} // anonymous namespace

std::vector<ThrowAnimation> AnimationPlanBuilder::buildThrowAnimations(
    const PlayResult& result) const {
    constexpr double ThrowSpeed     = 120.0; // ft/s
    constexpr double ReceptionDelay =  0.15; // s

    const int target = result.throwDecision.targetBase;
    if ((target == 0 && result.throwDecision.targetSequence.empty()) || result.fielderId < 0) return {};
    if (result.fieldingOutcome == FieldingOutcomeType::Error) return {};

    const Vector3 throwStart = clampPointInsideVisualFence(throwStartPoint(result));
    const double sx = throwStart.x;
    const double sy = throwStart.y;
    const double t0 = result.fielderTravelTime + ReceptionDelay;

    std::vector<ThrowAnimation> throws;

    std::vector<int> sequence = result.throwDecision.targetSequence;
    if (sequence.empty()) {
        if (result.throwDecision.doublePlay) {
            sequence = {2, 1};
        } else {
            sequence = {target};
        }
    }

    if (sequence.size() >= 2) {
        ThrowAnimation firstThrow = makeThrow(result.fielderId, result.fielderName,
                                             sequence.front(), sx, sy, t0, ThrowSpeed);
        throws.push_back(firstThrow);
        double nextStart = firstThrow.startTimeOffset + firstThrow.durationSeconds + 0.15;
        int previousBase = sequence.front();
        for (std::size_t i = 1; i < sequence.size(); ++i) {
            const int nextBase = sequence[i];
            throws.push_back(makeThrow(-1,
                                       std::to_string(previousBase) + "B",
                                       nextBase,
                                       kBases[previousBase].x,
                                       kBases[previousBase].y,
                                       nextStart,
                                       ThrowSpeed));
            nextStart = throws.back().startTimeOffset + throws.back().durationSeconds + 0.15;
            previousBase = nextBase;
        }
    } else {
        throws.push_back(makeThrow(result.fielderId, result.fielderName,
                                   sequence.front(), sx, sy, t0, ThrowSpeed));
    }

    return throws;
}

} // namespace joji
