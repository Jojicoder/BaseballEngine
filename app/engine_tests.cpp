#include "AtBatEngine.h"
#include "AnimationPlanBuilder.h"
#include "BallPhysicsEngine.h"
#include "PlayResolutionEngine.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>

namespace {

joji::Player createBatter() {
    return {"Test Batter", joji::Position::CenterField, 72, 64, 68, 74, 61, 55, 35, 34, 32};
}

joji::Player createPitcher() {
    return {"Test Pitcher", joji::Position::Pitcher, 41, 48, 45, 38, 50, 64, 82, 61, 77};
}

void testCountRules() {
    assert(!joji::isWalk({3, 0}));
    assert(joji::isWalk({4, 0}));
    assert(!joji::isStrikeOut({0, 2}));
    assert(joji::isStrikeOut({0, 3}));

    joji::Count count{0, 2};
    count = joji::afterFoul(count);
    assert(count.balls == 0);
    assert(count.strikes == 2);

    count = joji::afterBall({3, 1});
    assert(count.balls == 4);
    assert(count.strikes == 1);
}

void testAtBatTerminates() {
    joji::Random random{std::optional<std::uint32_t>{42}};
    joji::AtBatEngine engine;
    const joji::AtBatResult result = engine.simulatePlateAppearance(createBatter(), createPitcher(), random);

    assert(!result.pitchLogs.empty());
    assert(result.finalOutcome == joji::AtBatOutcome::Walk ||
           result.finalOutcome == joji::AtBatOutcome::StrikeOut ||
           result.finalOutcome == joji::AtBatOutcome::InPlay ||
           result.finalOutcome == joji::AtBatOutcome::HitByPitch);

    if (result.finalOutcome == joji::AtBatOutcome::InPlay) {
        assert(result.battedBallInput.has_value());
    }
}

void testBallPhysicsAndResolution() {
    joji::BallPhysicsEngine physics;
    joji::PlayResolutionEngine resolver;
    joji::GameState state;
    joji::Random random{std::optional<std::uint32_t>{7}};

    joji::BattedBallInput input;
    input.exitVelocity = 104.0;
    input.launchAngle = 26.0;
    input.sprayAngle = 0.0;
    input.backSpin = 2400.0;
    input.sideSpin = 0.0;

    const joji::BattedBall ball = physics.simulate(input);
    assert(ball.estimatedDistance > 250.0);
    assert(!ball.trajectory.empty());
    assert(!ball.bounceTrajectory.empty());
    assert(ball.hangTime > 0.0);
    assert(ball.maxHeight > 0.0);
    assert(ball.finalRestPoint.y >= ball.landingPoint.y);

    const joji::PlayResolution resolution = resolver.resolve(ball, state, random);
    assert(resolution.type == joji::PlayOutcomeType::Out ||
           resolution.type == joji::PlayOutcomeType::Single ||
           resolution.type == joji::PlayOutcomeType::Double ||
           resolution.type == joji::PlayOutcomeType::Triple ||
           resolution.type == joji::PlayOutcomeType::HomeRun ||
           resolution.type == joji::PlayOutcomeType::Foul);
}

void testAnimationPlanBuilder() {
    joji::AtBatResult atBat;
    atBat.batter = createBatter();
    atBat.pitcher = createPitcher();
    atBat.finalOutcome = joji::AtBatOutcome::InPlay;

    joji::PitchLog log;
    log.pitch.pitchType = joji::PitchType::Fastball;
    log.pitch.pitchVelocity = 92.0;
    log.pitch.locationX = 0.2;
    log.pitch.locationZ = 2.4;
    log.pitch.movementX = 4.0;
    log.pitch.movementZ = 10.0;
    log.pitchOutcome = joji::PitchOutcome::InPlay;
    atBat.pitchLogs.push_back(log);

    joji::BallPhysicsEngine physics;
    joji::BattedBallInput input;
    input.exitVelocity = 98.0;
    input.launchAngle = 18.0;
    input.sprayAngle = 8.0;
    input.backSpin = 2100.0;
    const joji::BattedBall ball = physics.simulate(input);

    joji::PlayResolution resolution;
    resolution.type = joji::PlayOutcomeType::Single;
    resolution.basesAwarded = 1;
    resolution.description = "base hit";

    joji::AnimationPlanBuilder builder;
    const joji::AnimationPlan plan = builder.build(atBat, resolution, ball, joji::DefenseAlignment{}, "test-game");

    assert(plan.gameId == "test-game");
    assert(plan.hasPitch);
    assert(plan.pitch.batterName == atBat.batter.name);
    assert(plan.pitch.pitcherName == atBat.pitcher.name);
    assert(plan.pitch.isInPlay);
    assert(plan.pitch.durationSeconds > 0.0);
    assert(!plan.pitch.points.empty());

    assert(plan.hasBattedBall);
    assert(plan.battedBall.result == "single");
    assert(plan.battedBall.durationSeconds >= ball.hangTime);
    assert(plan.battedBall.estimatedDistance == ball.estimatedDistance);
    assert(!plan.battedBall.points.empty());
    assert(plan.runners.empty());
    assert(plan.totalDurationSeconds >= plan.pitch.durationSeconds);
    assert(plan.totalDurationSeconds >= plan.battedBall.durationSeconds);
}

} // namespace

int main() {
    testCountRules();
    testAtBatTerminates();
    testBallPhysicsAndResolution();
    testAnimationPlanBuilder();

    std::cout << "engine_tests: ok\n";
    return 0;
}
