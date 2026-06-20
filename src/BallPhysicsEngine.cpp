#include "BallPhysicsEngine.h"

#include <algorithm>
#include <cmath>

namespace joji {

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double Gravity = 9.8;
constexpr double TimeStep = 0.016;
constexpr double FeetPerMeter = 3.280839895;
constexpr double MetersPerMph = 0.44704;
constexpr double Restitution = 0.45;
constexpr double LandFriction = 0.12;
constexpr double RollDeceleration = 3.5;
constexpr double RollStop = 0.25;
constexpr int MaxBounces = 4;

struct PhysicsState {
    Vector3 position;
    Vector3 velocity;
};

struct BallConfig {
    double mass = 0.145;
    double radius = 0.037;
    double dragCoefficient = 0.40;
    double airDensity = 1.225;
};

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

double lerp(double start, double end, double amount) {
    return start + (end - start) * amount;
}

double averageRandom(Random& random, int samples) {
    double total = 0.0;
    for (int i = 0; i < samples; ++i) {
        total += random.real(0.0, 1.0);
    }
    return total / samples;
}

double signedBell(Random& random, double center, double spread, int samples = 4) {
    return center + (averageRandom(random, samples) - 0.5) * 2.0 * spread;
}

double pullBiasedSpray(Random& random, double spread, double pullBias) {
    const double centerWeight = signedBell(random, 0.0, spread * 0.55, 5);
    const double edgeWeight = signedBell(random, pullBias, spread * 0.35, 3);
    return clamp(centerWeight * 0.72 + edgeWeight * 0.28, -48.0, 48.0);
}

double radialDistance(const Vector3& point) {
    return std::sqrt(point.x * point.x + point.y * point.y);
}

double sprayAngleDegrees(const Vector3& point) {
    return std::atan2(point.x, point.y) * 180.0 / Pi;
}

double fenceDistanceFeetAtAngle(double sprayAngle, const BallparkConfig& ballpark) {
    const double angle = clamp(std::abs(sprayAngle), 0.0, 45.0);
    if (angle <= 25.0) {
        return lerp(ballpark.centerFieldFenceFeet, ballpark.gapFenceFeet, angle / 25.0);
    }
    return lerp(ballpark.gapFenceFeet, ballpark.cornerFenceFeet, (angle - 25.0) / 20.0);
}

double fenceDistanceFeetForPoint(const Vector3& point, const BallparkConfig& ballpark) {
    return fenceDistanceFeetAtAngle(sprayAngleDegrees(point), ballpark);
}

bool isAtOrPastFence(const Vector3& point, const BallparkConfig& ballpark) {
    return radialDistance(point) >= fenceDistanceFeetForPoint(point, ballpark);
}

void fillFenceData(BattedBall& ball, const BallparkConfig& ballpark) {
    if (!ball.landsFair || ball.trajectory.size() < 2) {
        return;
    }

    for (std::size_t i = 1; i < ball.trajectory.size(); ++i) {
        const Vector3& previous = ball.trajectory[i - 1];
        const Vector3& current = ball.trajectory[i];
        if (!isAtOrPastFence(current, ballpark)) {
            continue;
        }

        const double previousFenceDistance = fenceDistanceFeetForPoint(previous, ballpark);
        const double currentFenceDistance = fenceDistanceFeetForPoint(current, ballpark);
        const double previousDelta = radialDistance(previous) - previousFenceDistance;
        const double currentDelta = radialDistance(current) - currentFenceDistance;
        const double denominator = previousDelta - currentDelta;
        const double amount = std::abs(denominator) > 0.0001
            ? clamp(previousDelta / denominator, 0.0, 1.0)
            : 1.0;

        ball.fenceIntersectionPoint = {
            lerp(previous.x, current.x, amount),
            lerp(previous.y, current.y, amount),
            lerp(previous.z, current.z, amount)
        };
        ball.fenceDistance = fenceDistanceFeetForPoint(ball.fenceIntersectionPoint, ballpark);
        ball.fenceCrossHeight = ball.fenceIntersectionPoint.z;
        ball.crossesFence = ball.fenceCrossHeight >= ballpark.fenceHeightFeet;
        return;
    }

    ball.fenceDistance = fenceDistanceFeetForPoint(ball.landingPoint, ballpark);
}

Vector3 add(const Vector3& left, const Vector3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vector3 multiply(const Vector3& value, double scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

double length2d(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

PhysicsState add(const PhysicsState& left, const PhysicsState& right) {
    return {add(left.position, right.position), add(left.velocity, right.velocity)};
}

PhysicsState multiply(const PhysicsState& state, double scalar) {
    return {multiply(state.position, scalar), multiply(state.velocity, scalar)};
}

bool isFoul(const Vector3& landingPoint) {
    return std::abs(landingPoint.x) > landingPoint.y;
}

bool shouldLandFair(AtBatResultType result) {
    switch (result) {
        case AtBatResultType::Single:
        case AtBatResultType::Double:
        case AtBatResultType::Triple:
        case AtBatResultType::HomeRun:
            return true;
        case AtBatResultType::StrikeOut:
        case AtBatResultType::Walk:
        case AtBatResultType::GroundOut:
        case AtBatResultType::FlyOut:
        case AtBatResultType::Error:
        case AtBatResultType::FielderChoice:
        case AtBatResultType::BuntFielderChoice:
        case AtBatResultType::InfieldFly:
        case AtBatResultType::SacrificeBunt:
        case AtBatResultType::SqueezeBunt:
        case AtBatResultType::BuntSingle:
            return false;
    }
    return false;
}

PhysicsState derivative(const PhysicsState& state, const BallConfig& config, double backSpinRpm, double sideSpin) {
    const double area = Pi * config.radius * config.radius;
    const double drag = 0.5 * config.airDensity * config.dragCoefficient * area / config.mass;
    const double liftCoefficient = 0.23 * (backSpinRpm / 2000.0);
    const double lift = 0.5 * config.airDensity * liftCoefficient * area / config.mass;

    const double speed = length(state.velocity);
    const double horizontalSpeed = std::sqrt(state.velocity.x * state.velocity.x + state.velocity.y * state.velocity.y);

    Vector3 acceleration = {0.0, 0.0, -Gravity};

    if (speed > 0.01) {
        acceleration.x -= drag * speed * state.velocity.x;
        acceleration.y -= drag * speed * state.velocity.y;
        acceleration.z -= drag * speed * state.velocity.z;
    }

    if (horizontalSpeed > 0.01) {
        const double liftAcceleration = lift * speed;
        acceleration.x += -liftAcceleration * state.velocity.x * state.velocity.z / horizontalSpeed;
        acceleration.y += -liftAcceleration * state.velocity.y * state.velocity.z / horizontalSpeed;
        acceleration.z += liftAcceleration * horizontalSpeed;

        const double sideAcceleration = 2.0 * sideSpin;
        acceleration.x += sideAcceleration * state.velocity.y / horizontalSpeed;
        acceleration.y += -sideAcceleration * state.velocity.x / horizontalSpeed;
    }

    return {state.velocity, acceleration};
}

PhysicsState rk4Step(const PhysicsState& state, const BallConfig& config, double backSpinRpm, double sideSpin) {
    PhysicsState k1 = derivative(state, config, backSpinRpm, sideSpin);
    PhysicsState k2 = derivative(add(state, multiply(k1, TimeStep / 2.0)), config, backSpinRpm, sideSpin);
    PhysicsState k3 = derivative(add(state, multiply(k2, TimeStep / 2.0)), config, backSpinRpm, sideSpin);
    PhysicsState k4 = derivative(add(state, multiply(k3, TimeStep)), config, backSpinRpm, sideSpin);

    return add(state, multiply(add(add(k1, multiply(k2, 2.0)), add(multiply(k3, 2.0), k4)), TimeStep / 6.0));
}

Vector3 interpolate(const Vector3& previous, const Vector3& current, double amount) {
    return {
        lerp(previous.x, current.x, amount),
        lerp(previous.y, current.y, amount),
        lerp(previous.z, current.z, amount)
    };
}

void addFeetPoint(std::vector<Vector3>& points, const Vector3& pointMeters) {
    points.push_back({
        pointMeters.x * FeetPerMeter,
        pointMeters.y * FeetPerMeter,
        pointMeters.z * FeetPerMeter
    });
}

void simulateBounceAndRoll(BattedBall& ball,
                           const Vector3& landingMeters,
                           const Vector3& landingVelocityMeters,
                           const BallConfig& config,
                           double backSpinRpm,
                           double sideSpin) {
    Vector3 position = landingMeters;
    Vector3 velocity = landingVelocityMeters;

    ball.bounceTrajectory.clear();
    addFeetPoint(ball.bounceTrajectory, position);

    for (int bounce = 0; bounce < MaxBounces; ++bounce) {
        velocity.z = -velocity.z * Restitution;
        velocity.x *= (1.0 - LandFriction);
        velocity.y *= (1.0 - LandFriction);

        if (std::abs(velocity.z) < 0.5) {
            break;
        }

        PhysicsState bounceState{{position.x, position.y, 0.0}, velocity};
        PhysicsState previous = bounceState;
        while (bounceState.position.z >= 0.0) {
            addFeetPoint(ball.bounceTrajectory, bounceState.position);
            previous = bounceState;
            bounceState = rk4Step(bounceState, config, backSpinRpm, sideSpin);
        }

        const double denominator = previous.position.z - bounceState.position.z;
        const double amount = std::abs(denominator) > 0.0001
            ? clamp(previous.position.z / denominator, 0.0, 1.0)
            : 1.0;
        position = interpolate(previous.position, bounceState.position, amount);
        position.z = 0.0;
        velocity = interpolate(previous.velocity, bounceState.velocity, amount);
        addFeetPoint(ball.bounceTrajectory, position);
    }

    double vx = velocity.x * (1.0 - LandFriction);
    double vy = velocity.y * (1.0 - LandFriction);

    while (true) {
        const double speed = std::sqrt(vx * vx + vy * vy);
        if (speed <= RollStop) {
            break;
        }

        addFeetPoint(ball.bounceTrajectory, {position.x, position.y, 0.0});

        const double deceleration = RollDeceleration * TimeStep;
        if (deceleration >= speed) {
            vx = 0.0;
            vy = 0.0;
            break;
        }

        const double scale = (speed - deceleration) / speed;
        vx *= scale;
        vy *= scale;
        position.x += vx * TimeStep;
        position.y += vy * TimeStep;
    }

    ball.finalRestPoint = {
        position.x * FeetPerMeter,
        position.y * FeetPerMeter,
        0.0
    };
    ball.groundRollDistance = length2d({
        ball.finalRestPoint.x - ball.landingPoint.x,
        ball.finalRestPoint.y - ball.landingPoint.y,
        0.0
    });
}

} // namespace

BattedBall BallPhysicsEngine::simulate(const BattedBallInput& input,
                                       const BallparkConfig& ballpark) const {
    const double launchRadians = input.launchAngle * Pi / 180.0;
    const double sprayRadians = input.sprayAngle * Pi / 180.0;
    const double exitVelocityMetersPerSecond = input.exitVelocity * MetersPerMph;

    PhysicsState state;
    state.position = {0.0, 0.0, 1.0};
    state.velocity = {
        exitVelocityMetersPerSecond * std::cos(launchRadians) * std::sin(sprayRadians),
        exitVelocityMetersPerSecond * std::cos(launchRadians) * std::cos(sprayRadians),
        exitVelocityMetersPerSecond * std::sin(launchRadians)
    };

    BallConfig config;
    std::vector<Vector3> trajectory;
    PhysicsState previous = state;
    double elapsed = 0.0;
    double maxHeightMeters = state.position.z;

    while (state.position.z >= 0.0 && elapsed < 12.0) {
        trajectory.push_back(state.position);
        previous = state;
        maxHeightMeters = std::max(maxHeightMeters, state.position.z);

        state = rk4Step(state, config, input.backSpin, input.sideSpin);
        elapsed += TimeStep;
    }

    const double interpolation = previous.position.z / (previous.position.z - state.position.z);
    const Vector3 landingMeters = {
        previous.position.x + interpolation * (state.position.x - previous.position.x),
        previous.position.y + interpolation * (state.position.y - previous.position.y),
        0.0
    };
    const Vector3 landingVelocityMeters = {
        previous.velocity.x + interpolation * (state.velocity.x - previous.velocity.x),
        previous.velocity.y + interpolation * (state.velocity.y - previous.velocity.y),
        previous.velocity.z + interpolation * (state.velocity.z - previous.velocity.z)
    };

    const double distanceMeters = std::sqrt(landingMeters.x * landingMeters.x + landingMeters.y * landingMeters.y);
    const bool foul = isFoul(landingMeters);

    BattedBall ball;
    ball.exitVelocity = input.exitVelocity;
    ball.launchAngle = input.launchAngle;
    ball.sprayAngle = input.sprayAngle;
    ball.sideSpin = input.sideSpin;
    ball.backSpin = input.backSpin;
    ball.estimatedDistance = clamp(distanceMeters * FeetPerMeter, 15.0, 470.0);
    ball.hangTime = elapsed;
    ball.maxHeight = maxHeightMeters * FeetPerMeter;
    ball.landingPoint = {
        landingMeters.x * FeetPerMeter,
        landingMeters.y * FeetPerMeter,
        0.0
    };
    ball.landingVelocity = {
        landingVelocityMeters.x * FeetPerMeter,
        landingVelocityMeters.y * FeetPerMeter,
        landingVelocityMeters.z * FeetPerMeter
    };
    ball.landsFair = !foul;
    ball.trajectory.reserve(trajectory.size());
    for (const auto& point : trajectory) {
        ball.trajectory.push_back({point.x * FeetPerMeter, point.y * FeetPerMeter, point.z * FeetPerMeter});
    }
    simulateBounceAndRoll(ball, landingMeters, landingVelocityMeters, config, input.backSpin, input.sideSpin);
    fillFenceData(ball, ballpark);
    ball.classification = classify(ball, foul, ballpark);
    ball.isBunt = input.isBunt;
    if (ball.isBunt) ball.classification = "bunt";
    return ball;
}

BattedBall BallPhysicsEngine::generateFromContact(int batterPower, int pitcherStuff, AtBatResultType result, Random& random) const {
    const double power = clamp(static_cast<double>(batterPower), 20.0, 100.0);
    const double stuff = clamp(static_cast<double>(pitcherStuff), 20.0, 100.0);
    const double rawContactQuality = clamp(0.52 + (power - 50.0) * 0.006 - (stuff - 50.0) * 0.0035, 0.18, 0.92);
    const double contactQuality = clamp(rawContactQuality + signedBell(random, 0.0, 0.22, 5), 0.05, 0.98);
    const double pullBias = random.chance(0.52) ? random.real(-14.0, -4.0) : random.real(4.0, 14.0);

    BattedBallInput input;
    input.sprayAngle = pullBiasedSpray(random, 42.0, pullBias);

    switch (result) {
        case AtBatResultType::GroundOut:
            input.exitVelocity = lerp(72.0, 105.0, contactQuality) + signedBell(random, 0.0, 5.5, 4);
            input.launchAngle = signedBell(random, -8.0, 11.0, 4);
            break;
        case AtBatResultType::FlyOut:
            input.exitVelocity = lerp(78.0, 104.0, contactQuality) + signedBell(random, 0.0, 5.0, 4);
            input.launchAngle = random.chance(0.22)
                ? signedBell(random, 48.0, 10.0, 4)
                : signedBell(random, 29.0, 11.0, 4);
            break;
        case AtBatResultType::Single:
            input.exitVelocity = lerp(76.0, 108.0, contactQuality) + signedBell(random, 0.0, 5.5, 4);
            input.launchAngle = random.chance(0.35)
                ? signedBell(random, 1.0, 8.0, 4)
                : signedBell(random, 12.0, 8.5, 4);
            break;
        case AtBatResultType::Double:
            input.exitVelocity = lerp(86.0, 112.0, contactQuality) + signedBell(random, 0.0, 4.5, 4);
            input.launchAngle = signedBell(random, 17.0, 10.0, 4);
            break;
        case AtBatResultType::Triple:
            input.exitVelocity = lerp(86.0, 111.0, contactQuality) + signedBell(random, 0.0, 4.5, 4);
            input.launchAngle = signedBell(random, 15.0, 9.0, 4);
            input.sprayAngle = random.chance(0.5)
                ? signedBell(random, -34.0, 10.0, 4)
                : signedBell(random, 34.0, 10.0, 4);
            break;
        case AtBatResultType::HomeRun:
            input.exitVelocity = lerp(96.0, 116.0, contactQuality) + signedBell(random, 0.0, 4.0, 4);
            input.launchAngle = signedBell(random, 27.0, 8.0, 4);
            break;
        case AtBatResultType::Walk:
        case AtBatResultType::StrikeOut:
        case AtBatResultType::Error:
        case AtBatResultType::FielderChoice:
        case AtBatResultType::BuntFielderChoice:
        case AtBatResultType::InfieldFly:
        case AtBatResultType::SacrificeBunt:
        case AtBatResultType::SqueezeBunt:
        case AtBatResultType::BuntSingle:
            return {};
    }

    input.exitVelocity = clamp(input.exitVelocity, 45.0, 123.0);
    input.launchAngle = clamp(input.launchAngle, -32.0, 62.0);
    input.sprayAngle = clamp(input.sprayAngle, -52.0, 52.0);
    input.backSpin = clamp(850.0 + input.launchAngle * 55.0 + input.exitVelocity * 8.0 + signedBell(random, 0.0, 420.0, 4),
                           450.0,
                           3600.0);
    if (input.launchAngle < 2.0) {
        input.backSpin = clamp(input.backSpin * 0.55, 250.0, 1500.0);
    }
    input.sideSpin = clamp(input.sprayAngle * 0.055 + signedBell(random, 0.0, 2.6, 4), -7.5, 7.5);

    BattedBall ball = simulate(input);
    for (int attempt = 0; attempt < 5 && shouldLandFair(result) && isFoul(ball.landingPoint); ++attempt) {
        input.sprayAngle *= 0.68;
        input.sideSpin *= 0.55;
        ball = simulate(input);
    }
    if (shouldLandFair(result) && isFoul(ball.landingPoint)) {
        input.sprayAngle = 0.0;
        input.sideSpin = 0.0;
        ball = simulate(input);
    }
    return ball;
}

std::string BallPhysicsEngine::classify(const BattedBall& ball, bool foul,
                                         const BallparkConfig& ballpark) {
    if (foul) {
        return "foul ball";
    }

    if (ball.crossesFence && ball.launchAngle >= 18.0) {
        return "deep drive";
    }
    if (ball.launchAngle < -5.0) {
        return "ground ball";
    }
    if (ball.launchAngle < 5.0) {
        return ball.maxHeight < 8.0 ? "hard ground ball" : "low liner";
    }
    if (ball.maxHeight < 18.0 && ball.launchAngle < 15.0) {
        return "line drive";
    }
    if (ball.launchAngle < 20.0) {
        return "line drive";
    }
    if (ball.launchAngle >= 42.0) {
        return "pop fly";
    }
    if (ball.estimatedDistance >= ballpark.centerFieldFenceFeet - 35.0
        || ball.fenceCrossHeight > 0.0) {
        return "warning track fly ball";
    }
    return "fly ball";
}

} // namespace joji
