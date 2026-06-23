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
    double windX = 0.0;  // m/s, positive = toward RF (+x field direction)
    double windY = 0.0;  // m/s, positive = toward CF (+y field direction)
};

// Standard atmosphere: density from altitude (ft) and temperature (°F)
double computeAirDensity(double altitudeFeet, double temperatureFahrenheit) {
    const double altMeters = altitudeFeet * 0.3048;
    const double tempKelvin = (temperatureFahrenheit - 32.0) * 5.0 / 9.0 + 273.15;
    const double pressurePa = 101325.0 * std::pow(1.0 - 2.2558e-5 * altMeters, 5.2559);
    return pressurePa / (287.05 * tempKelvin);
}

// 3D spin state: unit axis vector + spin rate (rpm)
struct SpinState {
    double axisX = 1.0;  // unit vector components
    double axisY = 0.0;
    double axisZ = 0.0;
    double rateRpm = 1800.0;
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

Vector3 cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Build a SpinState for a batted ball from legacy backSpin / sideSpin scalars.
// backSpin (rpm): vertical component — backspin creates lift, topspin adds drop.
// sideSpin (dimensionless, ±7.5 range): lateral component — scaled to ~70 rpm/unit
//   so that peak sidespin ≈ ±525 rpm matches the original ad-hoc force magnitude.
SpinState spinStateForBattedBall(double backSpinRpm, double sideSpin, double sprayAngleDeg) {
    const double sprayRad = sprayAngleDeg * Pi / 180.0;
    const double cosS     = std::cos(sprayRad);
    const double sinS     = std::sin(sprayRad);
    // 70 rpm per unit matches the legacy 2.0 * sideSpin lateral acceleration at typical speed
    const double sideRpm  = sideSpin * 70.0;
    const double total    = std::sqrt(backSpinRpm * backSpinRpm + sideRpm * sideRpm);

    SpinState spin;
    spin.rateRpm = total > 0.01 ? total : std::abs(backSpinRpm);
    if (total > 0.01) {
        // Backspin axis: perpendicular to flight direction in the horizontal plane.
        // For ball traveling in direction (sinS, cosS, 0), backspin creates upward lift
        // when axis = (cosS, -sinS, 0).  (Verified: cross((cosS,-sinS,0),(sinS,cosS,0)) = (0,0,1))
        const double bf = backSpinRpm / total;
        const double sf = sideRpm     / total;
        spin.axisX = cosS * bf;
        spin.axisY = -sinS * bf;
        // Positive sideSpin → breaks toward positive x (RF side for positive spray).
        // Need axisZ < 0 so that cross((0,0,axisZ),(0,1,0)) gives (+x,0,0).
        spin.axisZ = -sf;
    } else {
        spin.axisX = cosS;
        spin.axisY = -sinS;
        spin.axisZ = 0.0;
    }
    return spin;
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
        case AtBatResultType::HitByPitch:
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

// Magnus force direction: spinAxis × v_hat (cross product).
// Magnitude: Cl(rateRpm) * rho * A / (2m) * speed²
// Cl = 0.23 * (rateRpm / 2000) — calibrated to match Statcast lift data.
PhysicsState derivative(const PhysicsState& state, const BallConfig& config, const SpinState& spin) {
    const double area = Pi * config.radius * config.radius;
    const double drag = 0.5 * config.airDensity * config.dragCoefficient * area / config.mass;
    const double cl   = 0.23 * (spin.rateRpm / 2000.0);
    const double magnusCoeff = 0.5 * config.airDensity * cl * area / config.mass;

    const double speed = length(state.velocity);

    Vector3 acceleration = {0.0, 0.0, -Gravity};

    // Relative velocity of ball vs wind (drag acts on airflow over ball)
    const double relVx = state.velocity.x - config.windX;
    const double relVy = state.velocity.y - config.windY;
    const double relVz = state.velocity.z;
    const double relSpeed = std::sqrt(relVx*relVx + relVy*relVy + relVz*relVz);

    if (relSpeed > 0.01) {
        // Drag acts along relative velocity direction
        acceleration.x -= drag * relSpeed * relVx;
        acceleration.y -= drag * relSpeed * relVy;
        acceleration.z -= drag * relSpeed * relVz;
    }

    if (speed > 0.01) {
        // Magnus: a = magnusCoeff * speed² * (spinAxis × v_hat) — uses absolute velocity
        const Vector3 vHat = {state.velocity.x / speed,
                              state.velocity.y / speed,
                              state.velocity.z / speed};
        const Vector3 spinAxis = {spin.axisX, spin.axisY, spin.axisZ};
        const Vector3 magnusDir = cross(spinAxis, vHat);
        const double  magnusMag = magnusCoeff * speed * speed;
        acceleration.x += magnusDir.x * magnusMag;
        acceleration.y += magnusDir.y * magnusMag;
        acceleration.z += magnusDir.z * magnusMag;
    }

    return {state.velocity, acceleration};
}

PhysicsState rk4Step(const PhysicsState& state, const BallConfig& config, const SpinState& spin) {
    PhysicsState k1 = derivative(state, config, spin);
    PhysicsState k2 = derivative(add(state, multiply(k1, TimeStep / 2.0)), config, spin);
    PhysicsState k3 = derivative(add(state, multiply(k2, TimeStep / 2.0)), config, spin);
    PhysicsState k4 = derivative(add(state, multiply(k3, TimeStep)), config, spin);

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
                           const SpinState& spin) {
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
            bounceState = rk4Step(bounceState, config, spin);
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
    config.airDensity = computeAirDensity(ballpark.altitudeFeet, ballpark.temperatureFahrenheit);
    // Wind: convert mph to m/s; direction 0°=toward CF(tailwind), 90°=L→R, 180°=toward HP
    {
        const double windRad = ballpark.windDirectionDeg * Pi / 180.0;
        const double windMs  = ballpark.windSpeedMph * MetersPerMph;
        config.windX = windMs * std::sin(windRad);
        config.windY = windMs * std::cos(windRad);
    }

    const SpinState spin = spinStateForBattedBall(input.backSpin, input.sideSpin, input.sprayAngle);

    std::vector<Vector3> trajectory;
    PhysicsState previous = state;
    double elapsed = 0.0;
    double maxHeightMeters = state.position.z;

    // Spin decays ~6% per second (Statcast: ~5-8% for batted balls)
    constexpr double BattedBallSpinDecay = 0.06;

    while (state.position.z >= 0.0 && elapsed < 12.0) {
        trajectory.push_back(state.position);
        previous = state;
        maxHeightMeters = std::max(maxHeightMeters, state.position.z);

        SpinState decayedSpin = spin;
        decayedSpin.rateRpm = spin.rateRpm * std::exp(-BattedBallSpinDecay * elapsed);
        state = rk4Step(state, config, decayedSpin);
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
    simulateBounceAndRoll(ball, landingMeters, landingVelocityMeters, config, spin);
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
        case AtBatResultType::HitByPitch:
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
    // Physics-based spin: positive = backspin (lift), negative = topspin (drop)
    // tanh transition: grounders get topspin, fly balls get backspin
    const double spinMag = clamp(input.exitVelocity * 26.0 + signedBell(random, 0.0, 380.0, 4),
                                 700.0, 3400.0);
    const double spinDir = std::tanh((input.launchAngle - 4.0) / 10.0);  // -1=topspin, +1=backspin
    input.backSpin = spinMag * spinDir;
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
