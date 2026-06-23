#include "AnimationPlanBuilder.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace joji {

namespace {

constexpr double PitchDistanceFeet = 60.5;
constexpr double FeetPerMphSecond = 1.4666666667;
constexpr double PitcherReleaseHeightFeet = 6.0;
constexpr double DefaultBattedBallRollSpeedFeetPerSecond = 45.0;

AnimationPoint makePoint(const Vector3& point, double timeSeconds) {
    return {point.x, point.y, point.z, timeSeconds};
}

// ── Physical pitch trajectory simulation ──────────────────────────────────
// Coordinate system: origin = home plate, +y toward pitcher, +x = catcher's right, +z = up.
// Ball travels in -y direction from (0, 60.5, 6) to (locationX, 0, locationZ).
// All units: feet and ft/s.
//
// Physics: gravity + drag + Magnus (active spin) + seam-shift wake (SSW, gyro fraction).
// Calibrated Cl is 0.57× batted-ball value because at pitch speeds the spin-to-speed
// ratio S = ω*r/v is lower → empirically ~57% of the linear-rpm Magnus coefficient.

namespace pitch_physics {
constexpr double Gravity     = 32.174;      // ft/s²
constexpr double DragCoeff   = 0.002212;    // ft⁻¹  (derived from SI: 0.5*ρ*Cd*A/m / FtPerM)
constexpr double MagnusBase  = 0.000747;    // ft⁻¹ per 2000 rpm  (0.001308 * 0.571 calibration)
constexpr double SswFactor   = 0.55;        // SSW force relative to same-magnitude Magnus

struct State { double x, y, z, vx, vy, vz; };

State deriv(const State& s,
            double ax, double az,   // spin axis (unit, y≈0)
            double activeSpin,
            double rateRpm) {
    const double v2  = s.vx*s.vx + s.vy*s.vy + s.vz*s.vz;
    const double spd = std::sqrt(v2);
    if (spd < 0.1) return {s.vx, s.vy, s.vz, 0, 0, -Gravity};

    const double drag = DragCoeff * spd;
    const double mc   = MagnusBase * (rateRpm / 2000.0);

    // v̂
    const double ivx = s.vx/spd, ivy = s.vy/spd, ivz = s.vz/spd;

    // Magnus: activeSpin × (spinAxis × v̂)
    // spinAxis = (ax, 0, az); cross with v̂:
    //   (ax,0,az) × (ivx,ivy,ivz) = (0*ivz - az*ivy,  az*ivx - ax*ivz,  ax*ivy - 0*ivx)
    const double mDirX = -az * ivy;
    const double mDirY =  az * ivx - ax * ivz;
    const double mDirZ =  ax * ivy;
    const double magnusMag = mc * v2 * activeSpin;

    // SSW: (1-activeSpin) fraction drives wake force perpendicular to velocity in
    // the horizontal plane, in the direction of (spinAxis projected to horizontal × ŷ).
    // Simplified: SSW acts in the same direction as Magnus horizontal component.
    const double sswMag = mc * v2 * (1.0 - activeSpin) * SswFactor;
    // SSW direction: horizontal projection of Magnus direction (keep x, zero y, keep z but halved)
    const double sswX = mDirX;
    const double sswZ = mDirZ * 0.30;  // less vertical contribution from seam wake

    const double ax_ = -drag*s.vx + (magnusMag*mDirX + sswMag*sswX);
    const double ay_ = -drag*s.vy + (magnusMag*mDirY);
    const double az_ = -Gravity - drag*s.vz + (magnusMag*mDirZ + sswMag*sswZ);

    return {s.vx, s.vy, s.vz, ax_, ay_, az_};
}

State rk4(const State& s, double dt,
          double ax, double az, double activeSpin, double rateRpm) {
    auto add = [](const State& a, const State& b, double h) -> State {
        return {a.x+b.x*h, a.y+b.y*h, a.z+b.z*h,
                a.vx+b.vx*h, a.vy+b.vy*h, a.vz+b.vz*h};
    };
    State k1 = deriv(s, ax, az, activeSpin, rateRpm);
    State k2 = deriv(add(s, k1, dt/2), ax, az, activeSpin, rateRpm);
    State k3 = deriv(add(s, k2, dt/2), ax, az, activeSpin, rateRpm);
    State k4 = deriv(add(s, k3, dt  ), ax, az, activeSpin, rateRpm);
    return {
        s.x  + (k1.x +2*k2.x +2*k3.x +k4.x)*dt/6,
        s.y  + (k1.y +2*k2.y +2*k3.y +k4.y)*dt/6,
        s.z  + (k1.z +2*k2.z +2*k3.z +k4.z)*dt/6,
        s.vx + (k1.vx+2*k2.vx+2*k3.vx+k4.vx)*dt/6,
        s.vy + (k1.vy+2*k2.vy+2*k3.vy+k4.vy)*dt/6,
        s.vz + (k1.vz+2*k2.vz+2*k3.vz+k4.vz)*dt/6
    };
}
} // namespace pitch_physics

double pitchDurationSeconds(const Pitch& pitch) {
    // Fastballs look noticeably sharper than breaking balls.
    // Offspeed pitches keep their deceptive slowness relative to heaters.
    double boost;
    switch (pitch.pitchType) {
        case PitchType::Fastball:  boost = 3.5; break;  // 95 mph → ~0.12s
        case PitchType::Cutter:    boost = 2.0; break;
        case PitchType::Curveball: boost = 0.9; break;  // slow arc — drop looks gradual
        default:                   boost = 1.4; break;  // slider/change/splitter
    }
    const double velocityFeetPerSecond = std::max(1.0, pitch.pitchVelocity * FeetPerMphSecond * boost);
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

// 壁接触後のカロム軌道を生成（アニメーション専用、試合結果に影響しない）
std::vector<Vector3> computeWallCaromPoints(const BattedBall& ball,
                                            const std::vector<Vector3>& flightTraj) {
    if (flightTraj.size() < 2) return {};

    // 末尾2点から壁到達時の速度を推定 (feet/second)
    constexpr double dt = 0.016;
    const Vector3& p1 = flightTraj[flightTraj.size() - 2];
    const Vector3& p2 = flightTraj[flightTraj.size() - 1];
    double vx = (p2.x - p1.x) / dt;
    double vy = (p2.y - p1.y) / dt;
    double vz = (p2.z - p1.z) / dt;

    // 壁法線: 内向き径方向
    const double ix = ball.fenceIntersectionPoint.x;
    const double iy = ball.fenceIntersectionPoint.y;
    const double r = std::sqrt(ix * ix + iy * iy);
    if (r < 0.001) return {};
    const double nx = -ix / r;
    const double ny = -iy / r;

    // 法線方向速度を反射・減衰させる
    constexpr double WallRestitution = 0.38;
    const double dot = vx * nx + vy * ny;
    vx = (vx - 2.0 * dot * nx) * WallRestitution;
    vy = (vy - 2.0 * dot * ny) * WallRestitution;
    vz = std::abs(vz) * 0.12; // 壁から離れる小さい上方成分

    // 壁接触点から放物線シミュレーション (z >= 0 まで)
    constexpr double g = 32.2; // ft/s^2
    constexpr int MaxSteps = 400;
    double x = ball.fenceIntersectionPoint.x;
    double y = ball.fenceIntersectionPoint.y;
    double z = std::max(ball.fenceIntersectionPoint.z, 0.5);

    std::vector<Vector3> carom;
    carom.push_back({x, y, z});
    for (int i = 0; i < MaxSteps; ++i) {
        vz -= g * dt;
        x += vx * dt;
        y += vy * dt;
        z += vz * dt;
        if (z <= 0.0) {
            carom.push_back({x, y, 0.0});
            break;
        }
        carom.push_back({x, y, z});
    }
    return carom;
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

std::vector<Vector3> clippedTrajectoryThrough(const std::vector<Vector3>& trajectory,
                                              const Vector3& target) {
    if (trajectory.empty()) {
        return {};
    }
    const std::size_t closest = closestTrajectoryIndex(trajectory, target);
    std::vector<Vector3> clipped;
    clipped.reserve(closest + 2);
    for (std::size_t i = 0; i <= closest; ++i) {
        clipped.push_back(trajectory[i]);
    }
    if (distanceSquared2d(clipped.back(), target) > 0.25) {
        clipped.push_back(target);
    }
    return clipped;
}

bool hasFenceIntersection(const BattedBall& ball) {
    return std::abs(ball.fenceIntersectionPoint.x) > 0.001
        || std::abs(ball.fenceIntersectionPoint.y) > 0.001
        || std::abs(ball.fenceIntersectionPoint.z) > 0.001;
}

bool isWallContact(const BattedBall& ball) {
    return ball.fenceCrossHeight > 0.0
        && !ball.crossesFence
        && hasFenceIntersection(ball);
}

double timeAtClosestTrajectoryPoint(const std::vector<Vector3>& trajectory,
                                    const Vector3& target,
                                    double durationSeconds) {
    if (trajectory.size() <= 1) {
        return durationSeconds;
    }
    const std::size_t closest = closestTrajectoryIndex(trajectory, target);
    const double amount = static_cast<double>(closest)
        / static_cast<double>(trajectory.size() - 1);
    return durationSeconds * amount;
}

// 物理フェンスと同じ極座標モデルで、x座標における最大 y を返す（汎用デフォルト値使用）
double visualFenceY(double xFeet) {
    // Generic ballpark: CF=400, gap=365 at 25°, corner=325 at 45°
    // atan2(|x|, y) で角度を求めて R を計算 → 連立を解く（簡易近似）
    // 精度十分な近似: angle ≈ atan2(|x|, CF) で R を求め、y = sqrt(R²-x²)
    constexpr double CF = 400.0, gap = 365.0, corner = 325.0;
    const double absX = std::abs(xFeet);
    if (absX >= CF) return 0.0;
    constexpr double Pi = 3.14159265358979;
    const double approxAngle = std::atan2(absX, CF) * 180.0 / Pi;
    const double a = std::min(approxAngle, 45.0);
    const double r = a <= 25.0 ? CF + (gap - CF) * (a / 25.0)
                               : gap + (corner - gap) * ((a - 25.0) / 20.0);
    return std::sqrt(std::max(0.0, r * r - absX * absX));
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
    rebuildDerivedTimeline(plan, nullptr);

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

    // Physical simulation: RK4 from release to home plate.
    // Initial velocity: mostly toward plate (-y), with vx/vz aimed at plate location
    // plus a bias to roughly compensate for expected break.
    {
        using namespace pitch_physics;
        constexpr double dt = 0.004;  // 4 ms for smooth trajectory

        const double vTotal = pitch.pitchVelocity * FeetPerMphSecond;
        // Aim at plate straight line (compensated for gravity drop at mid-flight)
        const double dY   = PitchDistanceFeet;
        const double dX   = pitch.locationX - 0.0;           // horizontal
        const double dZ   = pitch.locationZ - PitcherReleaseHeightFeet; // vertical
        const double tEst = dY / vTotal;
        // Adjust vx/vz so the straight-line aim accounts for gravity halfway
        const double vz0 = dZ / tEst + 0.5 * Gravity * tEst;
        const double vx0 = dX / tEst;
        const double vy0 = -std::sqrt(std::max(0.0, vTotal*vTotal - vx0*vx0 - vz0*vz0));

        State s{0.0, PitchDistanceFeet, PitcherReleaseHeightFeet, vx0, vy0, vz0};
        double t = 0.0;
        animation.points.push_back({s.x, s.y, s.z, t});

        // Spin decay: pitches lose ~10% per second (gyroscopic precession + seam turbulence)
        constexpr double PitchSpinDecay = 0.10;
        while (s.y > 0.0 && t < 1.0) {
            const double decayedRpm = pitch.spinRate * std::exp(-PitchSpinDecay * t);
            s = rk4(s, dt, pitch.spinAxisX, pitch.spinAxisZ,
                    pitch.activeSpin, decayedRpm);
            t += dt;
            animation.points.push_back({s.x, s.y, s.z, t});
        }

        // Snap last point to exact plate location so the end-zone display is correct
        if (!animation.points.empty()) {
            auto& last = animation.points.back();
            last.x = pitch.locationX;
            last.y = 0.0;
            last.z = pitch.locationZ;
            last.timeSeconds = animation.durationSeconds;
        }
        animation.end = animation.points.empty() ? animation.end : animation.points.back();
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
    if (battedBall.crossesFence && hasFenceIntersection(battedBall)) {
        animation.fenceCrossSeconds = timeAtClosestTrajectoryPoint(
            battedBall.trajectory, battedBall.fenceIntersectionPoint, battedBall.hangTime);
    }
    Vector3 visualLanding = battedBall.landingPoint;
    double visualHangTime = battedBall.hangTime;
    std::vector<Vector3> visualTrajectory = battedBall.trajectory;
    if (isWallContact(battedBall)) {
        visualLanding = battedBall.fenceIntersectionPoint;
        visualHangTime = timeAtClosestTrajectoryPoint(battedBall.trajectory,
                                                      visualLanding,
                                                      battedBall.hangTime);
        visualTrajectory = clippedTrajectoryThrough(battedBall.trajectory, visualLanding);
    }
    animation.landingPoint = makePoint(visualLanding, visualHangTime);

    if (battedBall.trajectory.empty()) {
        animation.finalPoint = animation.landingPoint;
        return animation;
    }

    appendTimedTrajectory(animation.points,
                          visualTrajectory,
                          0.0,
                          visualHangTime);

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

    if (isWallContact(battedBall)) {
        // 壁到達点まで飛行アニメーションを確定
        animation.finalPoint = makePoint(visualLanding, visualHangTime);
        if (animation.points.empty()
            || animation.points.back().x != animation.finalPoint.x
            || animation.points.back().y != animation.finalPoint.y
            || animation.points.back().z != animation.finalPoint.z) {
            animation.points.push_back(animation.finalPoint);
        }

        // 壁から跳ね返るカロム軌道を追加
        const auto caromPts = computeWallCaromPoints(battedBall, visualTrajectory);
        if (!caromPts.empty()) {
            const double caromDuration = std::max(0.4,
                static_cast<double>(caromPts.size()) * 0.016);
            appendTimedTrajectory(animation.points, caromPts, visualHangTime, caromDuration);
            animation.durationSeconds = visualHangTime + caromDuration;
            animation.finalPoint = makePoint(caromPts.back(), animation.durationSeconds);
        } else {
            animation.durationSeconds = visualHangTime;
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

    // If a fielder is assigned, clip the roll animation at the moment they arrive.
    // Without this, the ball visually rolls to its final rest in deep outfield even
    // though the fielder intercepts it much earlier — making singles look like doubles.
    if (resolution.fielderId >= 0
        && !battedBall.bounceTrajectory.empty()
        && bounceDuration > 0.0) {
        const double endTime = std::max(battedBall.hangTime, resolution.fielderTravelTime + 0.08);
        const double relTime = std::clamp(endTime - battedBall.hangTime, 0.0, bounceDuration);
        const std::size_t n = battedBall.bounceTrajectory.size();
        const std::size_t clipIdx = std::min(
            static_cast<std::size_t>(std::round(relTime / bounceDuration * static_cast<double>(n - 1))),
            n - 1);
        std::vector<Vector3> clipped(battedBall.bounceTrajectory.begin(),
                                     battedBall.bounceTrajectory.begin() + clipIdx + 1);
        appendTimedTrajectory(animation.points, clipped,
                              battedBall.hangTime, endTime - battedBall.hangTime);
        animation.durationSeconds = endTime;
        animation.finalPoint = makePoint(battedBall.bounceTrajectory[clipIdx], endTime);
        if (animation.points.empty()
            || animation.points.back().x != animation.finalPoint.x
            || animation.points.back().y != animation.finalPoint.y
            || animation.points.back().z != animation.finalPoint.z) {
            animation.points.push_back(animation.finalPoint);
        }
        return animation;
    }

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

// Arc height scales with throw distance: ~7% of distance, 4–25 ft range.
double throwArcHeight(double distanceFeet) {
    return std::clamp(distanceFeet * 0.07, 4.0, 25.0);
}

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
    t.arcHeightFeet   = throwArcHeight(dist);
    t.points.push_back({fromX, fromY, 0.0, startOffset});
    t.points.push_back({to.x,  to.y,  0.0, startOffset + t.durationSeconds});
    return t;
}

ThrowAnimation makeThrowToPoint(int fielderId, const std::string& fielderName,
                                double toX, double toY,
                                double fromX, double fromY,
                                double startOffset,
                                double throwSpeed) {
    const double dist = std::sqrt((toX - fromX) * (toX - fromX) +
                                  (toY - fromY) * (toY - fromY));
    ThrowAnimation t;
    t.fielderId       = fielderId;
    t.fielderName     = fielderName;
    t.targetBase      = 0;
    t.startTimeOffset = startOffset;
    t.durationSeconds = dist / throwSpeed;
    t.arcHeightFeet   = throwArcHeight(dist);
    t.points.push_back({fromX, fromY, 0.0, startOffset});
    t.points.push_back({toX,   toY,   0.0, startOffset + t.durationSeconds});
    return t;
}

void applyBadThrowVisual(ThrowAnimation& throwAnim, const ThrowDecision& decision) {
    if (!decision.badThrow || throwAnim.points.size() < 2) {
        return;
    }
    AnimationPoint& end = throwAnim.points.back();
    const AnimationPoint& start = throwAnim.points.front();
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::max(0.001, std::sqrt(dx * dx + dy * dy));
    const double side = decision.targetBase == 3 ? -1.0 : 1.0;
    end.x += (-dy / length) * decision.offlineFeet * side;
    end.y += ( dx / length) * decision.offlineFeet * side;
    end.timeSeconds += decision.arrivalDelaySeconds;
    throwAnim.durationSeconds += decision.arrivalDelaySeconds;
    throwAnim.badThrow = true;
    throwAnim.offlineFeet = decision.offlineFeet;
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
    const double t0 = std::max(result.fielderTravelTime,
                               result.fieldingAvailableTime) + ReceptionDelay;

    std::vector<ThrowAnimation> throws;

    if (result.throwDecision.useCutoff && target > 0) {
        const Vector3 cutoff = clampPointInsideVisualFence(result.throwDecision.cutoffPoint);
        ThrowAnimation firstThrow = makeThrowToPoint(result.fielderId,
                                                     result.fielderName,
                                                     cutoff.x,
                                                     cutoff.y,
                                                     sx,
                                                     sy,
                                                     t0,
                                                     ThrowSpeed);
        throws.push_back(firstThrow);
        const double nextStart = firstThrow.startTimeOffset + firstThrow.durationSeconds + 0.15;
        throws.push_back(makeThrow(-1,
                                   result.throwDecision.cutoffFielderName.empty()
                                       ? "CUT" : result.throwDecision.cutoffFielderName,
                                   target,
                                   cutoff.x,
                                   cutoff.y,
                                   nextStart,
                                   ThrowSpeed));
        applyBadThrowVisual(throws.back(), result.throwDecision);
        return throws;
    }

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
    if (!throws.empty()) {
        applyBadThrowVisual(throws.back(), result.throwDecision);
    }

    return throws;
}

std::vector<RunnerMovement> AnimationPlanBuilder::buildRunnerMovements(
    const std::vector<RunnerAnimation>& runners) const {
    std::vector<RunnerMovement> movements;
    movements.reserve(runners.size());
    for (const RunnerAnimation& runner : runners) {
        if (runner.points.empty()) {
            continue;
        }
        RunnerMovement movement;
        movement.runnerName = runner.runnerName;
        movement.fromBase = runner.fromBase;
        movement.toBase = runner.toBase;
        movement.startTime = runner.points.front().timeSeconds;
        movement.arrivalTime = runner.points.back().timeSeconds;
        movement.topSpeedFeetPerSecond = 26.0;
        movement.accelerationFeetPerSecond2 = 18.0;
        movement.turnPenaltySeconds = std::max(0, runner.toBase - runner.fromBase - 1) * 0.12;
        movement.slideTimeSeconds = runner.out || runner.toBase >= 2 ? 0.28 : 0.0;
        movement.usedForSafeOut = true;
        movement.scored = runner.scored;
        movement.out = runner.out;
        movement.routePoints = runner.points;
        movements.push_back(movement);
    }
    return movements;
}

std::vector<ThrowMovement> AnimationPlanBuilder::buildThrowMovements(
    const std::vector<ThrowAnimation>& throws) const {
    std::vector<ThrowMovement> movements;
    movements.reserve(throws.size());
    for (const ThrowAnimation& throwAnim : throws) {
        if (throwAnim.points.size() < 2) {
            continue;
        }
        const AnimationPoint& start = throwAnim.points.front();
        const AnimationPoint& end = throwAnim.points.back();
        const double duration = std::max(0.001, throwAnim.durationSeconds);
        const double dx = end.x - start.x;
        const double dy = end.y - start.y;
        const double distance = std::sqrt(dx * dx + dy * dy);

        ThrowMovement movement;
        movement.fielderName = throwAnim.fielderName;
        movement.fromPosition = start;
        movement.targetBase = throwAnim.targetBase;
        movement.startTime = throwAnim.startTimeOffset;
        movement.arrivalTime = throwAnim.startTimeOffset + throwAnim.durationSeconds;
        movement.velocityFeetPerSecond = distance / duration;
        movement.accuracy = throwAnim.badThrow ? 0.62 : 0.92;
        movement.trajectory = throwAnim.points;
        movements.push_back(movement);
    }
    return movements;
}

std::vector<TagPlay> AnimationPlanBuilder::buildTagPlays(
    const PlayResult& result,
    const std::vector<RunnerMovement>& runners,
    const std::vector<ThrowMovement>& throws) const {
    std::vector<TagPlay> tags = result.tagPlays;
    auto remapToDisplayTime = [&](TagPlay& tag) {
        tag.tagTime = 0.18;
        for (const RunnerMovement& runner : runners) {
            if (runner.runnerName == tag.runnerName && runner.toBase == tag.base) {
                tag.runnerArrivalTime = runner.arrivalTime;
                break;
            }
        }
        tag.ballArrivalTime = tag.runnerArrivalTime;
        for (const ThrowMovement& throwMovement : throws) {
            if (throwMovement.targetBase == tag.base) {
                tag.ballArrivalTime = throwMovement.arrivalTime;
                break;
            }
        }
        if (!tag.runnerSafe
            && tag.ballArrivalTime + tag.tagTime >= tag.runnerArrivalTime) {
            tag.runnerArrivalTime = tag.ballArrivalTime + tag.tagTime + 0.05;
        } else if (tag.runnerSafe
                   && tag.ballArrivalTime + tag.tagTime < tag.runnerArrivalTime) {
            tag.ballArrivalTime = std::max(0.0, tag.runnerArrivalTime - tag.tagTime + 0.05);
        }
    };

    for (TagPlay& tag : tags) {
        remapToDisplayTime(tag);
    }

    for (const RunnerOut& runnerOut : result.runnerOuts) {
        const bool alreadyTracked = std::any_of(
            tags.begin(), tags.end(), [&](const TagPlay& tag) {
                return tag.runnerName == runnerOut.runnerName
                    && tag.base == runnerOut.outAtBase;
            });
        if (alreadyTracked) {
            continue;
        }

        TagPlay tag;
        tag.base = runnerOut.outAtBase;
        tag.runnerName = runnerOut.runnerName;
        tag.runnerSafe = false;
        remapToDisplayTime(tag);
        tags.push_back(tag);
    }
    return tags;
}

ReplayTimeline AnimationPlanBuilder::buildReplayTimeline(const AnimationPlan& plan) const {
    ReplayTimeline timeline;
    auto push = [&](ReplayEventType type,
                    double timeSeconds,
                    std::string label,
                    std::string actor = "",
                    std::string detail = "",
                    std::string result = "",
                    int base = 0,
                    AnimationPoint position = {}) {
        ReplayEvent event;
        event.type = type;
        event.timeSeconds = std::max(0.0, timeSeconds);
        event.label = std::move(label);
        event.actor = std::move(actor);
        event.detail = std::move(detail);
        event.result = std::move(result);
        event.base = base;
        event.position = position;
        timeline.events.push_back(event);
        timeline.durationSeconds = std::max(timeline.durationSeconds, event.timeSeconds);
    };

    if (plan.hasPitch) {
        push(ReplayEventType::Pitch,
             0.0,
             "pitch released",
             plan.pitch.pitcherName,
             plan.pitch.pitchType + " " + std::to_string(static_cast<int>(plan.pitch.velocity)) + " mph",
             "",
             0,
             plan.pitch.start);
        if (!plan.pitch.points.empty()) {
            for (std::size_t i = 1; i + 1 < plan.pitch.points.size(); ++i) {
                push(ReplayEventType::Pitch,
                     plan.pitch.points[i].timeSeconds,
                     "pitch in flight",
                     plan.pitch.pitcherName,
                     plan.pitch.pitchType,
                     "",
                     0,
                     plan.pitch.points[i]);
            }
        }
        push(ReplayEventType::Contact,
             plan.pitch.durationSeconds,
             plan.pitch.isInPlay ? "contact" : "pitch received",
             plan.pitch.batterName,
             "zone " + std::to_string(plan.pitch.zoneNumber),
             plan.pitch.result,
             0,
             plan.pitch.end);
    }
    if (plan.hasBattedBall) {
        if (!plan.battedBall.points.empty()) {
            push(ReplayEventType::BallFlight,
                 0.0,
                 "ball leaves bat",
                 plan.battedBall.batterName,
                 plan.battedBall.classification,
                 "",
                 0,
                 plan.battedBall.points.front());
            const std::size_t midpoint = plan.battedBall.points.size() / 2;
            if (midpoint > 0 && midpoint < plan.battedBall.points.size()) {
                push(ReplayEventType::BallFlight,
                     plan.battedBall.points[midpoint].timeSeconds,
                     "ball in flight",
                     plan.battedBall.batterName,
                     plan.battedBall.classification,
                     "",
                     0,
                     plan.battedBall.points[midpoint]);
            }
        }
        push(ReplayEventType::BallFlight,
             plan.battedBall.durationSeconds,
             plan.battedBall.landsFair ? "ball lands fair" : "ball lands foul",
             plan.battedBall.batterName,
             std::to_string(static_cast<int>(plan.battedBall.estimatedDistance)) + " ft",
             plan.battedBall.result,
             0,
             plan.battedBall.landingPoint);
        push(ReplayEventType::Field,
             plan.battedBall.durationSeconds,
             "ball ready to field",
             "",
             plan.battedBall.classification,
             plan.battedBall.result,
             0,
             plan.battedBall.finalPoint);
    }
    for (const DefenseAnimation& defender : plan.defenders) {
        if (!defender.points.empty()) {
            push(ReplayEventType::Field,
                 defender.points.front().timeSeconds,
                 "fielder reacts",
                 defender.fielderName,
                 defender.madePlay ? "fielding play" : "route",
                 "",
                 0,
                 defender.points.front());
            push(ReplayEventType::Field,
                 defender.durationSeconds,
                 defender.madePlay ? "fielder reaches ball" : "fielder route ends",
                 defender.fielderName,
                 "",
                 defender.madePlay ? "made play" : "no play",
                 0,
                defender.points.back());
        }
    }
    if (!plan.defensiveDecision.reason.empty()) {
        const double decisionTime = !plan.throwMovements.empty()
            ? std::max(0.0, plan.throwMovements.front().startTime - 0.08)
            : (plan.hasBattedBall ? plan.battedBall.durationSeconds + 0.05 : 0.0);
        push(ReplayEventType::Field,
             decisionTime,
             plan.defensiveDecision.holdBall ? "defense holds ball" : "defensive decision",
             "",
             plan.defensiveDecision.reason,
             plan.defensiveDecision.holdBall
                 ? "hold"
                 : "throw " + std::to_string(plan.defensiveDecision.chosenTargetBase) + "B"
                   + (plan.defensiveDecision.cutoff ? " via cutoff" : ""),
             plan.defensiveDecision.chosenTargetBase);
    }
    for (const ThrowMovement& throwMovement : plan.throwMovements) {
        push(ReplayEventType::Throw,
             throwMovement.startTime,
             "throw released",
             throwMovement.fielderName,
             std::to_string(static_cast<int>(throwMovement.velocityFeetPerSecond)) + " ft/s",
             "",
             throwMovement.targetBase,
             throwMovement.fromPosition);
        const AnimationPoint target = throwMovement.trajectory.empty()
            ? AnimationPoint{} : throwMovement.trajectory.back();
        push(ReplayEventType::Throw,
             throwMovement.arrivalTime,
             "throw arrives",
             throwMovement.fielderName,
             "accuracy " + std::to_string(static_cast<int>(throwMovement.accuracy * 100.0)) + "%",
             "",
             throwMovement.targetBase,
             target);
    }
    for (const RunnerMovement& runner : plan.runnerMovements) {
        const AnimationPoint start = runner.routePoints.empty()
            ? AnimationPoint{} : runner.routePoints.front();
        const AnimationPoint end = runner.routePoints.empty()
            ? AnimationPoint{} : runner.routePoints.back();
        push(ReplayEventType::Runner,
             runner.startTime,
             "runner breaks",
             runner.runnerName,
             std::to_string(runner.fromBase) + "B to " + std::to_string(runner.toBase) + "B",
             "",
             runner.fromBase,
             start);
        push(ReplayEventType::Runner,
             runner.arrivalTime,
             runner.out ? "runner reaches tag point"
                        : runner.scored ? "runner scores" : "runner arrives",
             runner.runnerName,
             "",
             runner.out ? "out pending" : runner.scored ? "scored" : "safe pending",
             runner.toBase,
             end);
    }
    for (const TagPlay& tag : plan.tagPlays) {
        push(ReplayEventType::Tag,
             std::max(tag.runnerArrivalTime, tag.ballArrivalTime) + tag.tagTime,
             tag.runnerSafe ? "tag play safe" : "tag play out",
             tag.runnerName,
             "runner " + std::to_string(static_cast<int>(tag.runnerArrivalTime * 100.0) / 100.0)
                 + " / ball " + std::to_string(static_cast<int>(tag.ballArrivalTime * 100.0) / 100.0),
             tag.runnerSafe ? "safe" : "out",
             tag.base);
    }
    push(ReplayEventType::Result,
         plan.totalDurationSeconds,
         "play complete",
         "",
         "",
         plan.description);
    std::sort(timeline.events.begin(), timeline.events.end(),
              [](const ReplayEvent& left, const ReplayEvent& right) {
                  if (left.timeSeconds != right.timeSeconds) {
                      return left.timeSeconds < right.timeSeconds;
                  }
                  return static_cast<int>(left.type) < static_cast<int>(right.type);
              });
    timeline.durationSeconds = std::max(timeline.durationSeconds, plan.totalDurationSeconds);
    return timeline;
}

void AnimationPlanBuilder::rebuildDerivedTimeline(AnimationPlan& plan,
                                                  const PlayResult* result) const {
    plan.runnerMovements = buildRunnerMovements(plan.runners);
    plan.throwMovements = buildThrowMovements(plan.throws);
    plan.tagPlays = result != nullptr
        ? buildTagPlays(*result, plan.runnerMovements, plan.throwMovements)
        : std::vector<TagPlay>{};
    plan.defensiveDecision = {};
    if (result != nullptr) {
        plan.defensiveDecision.chosenTargetBase = result->defensiveDecision.chosenTargetBase;
        plan.defensiveDecision.holdBall = result->defensiveDecision.holdBall;
        plan.defensiveDecision.cutoff = result->defensiveDecision.cutoff
            || result->throwDecision.useCutoff;
        plan.defensiveDecision.reason = result->defensiveDecision.reason;
        if (plan.defensiveDecision.reason.empty()
            && result->throwDecision.targetBase > 0) {
            plan.defensiveDecision.chosenTargetBase = result->throwDecision.targetBase;
            plan.defensiveDecision.holdBall = false;
            plan.defensiveDecision.reason = "throw to "
                + std::to_string(result->throwDecision.targetBase) + "B";
        }
    }
    plan.replayTimeline = buildReplayTimeline(plan);
    plan.totalDurationSeconds = std::max(plan.totalDurationSeconds,
                                         plan.replayTimeline.durationSeconds);
}

} // namespace joji
