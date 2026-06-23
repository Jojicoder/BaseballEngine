#include "AtBatEngine.h"
#include "AnimationPlanBuilder.h"
#include "BallPhysicsEngine.h"
#include "JsonExporter.h"
#include "PlayResolutionEngine.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

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

void testFieldingPhysicsBoundaries() {
    joji::PlayResolutionEngine resolver;
    joji::GameState state;

    joji::BattedBall deepGrounder;
    deepGrounder.exitVelocity = 105.0;
    deepGrounder.launchAngle = -2.0;
    deepGrounder.sprayAngle = 0.0;
    deepGrounder.estimatedDistance = 245.0;
    deepGrounder.hangTime = 0.18;
    deepGrounder.maxHeight = 2.0;
    deepGrounder.landsFair = true;
    deepGrounder.classification = "hard ground ball";
    deepGrounder.landingPoint = {0.0, 38.0, 0.0};
    deepGrounder.finalRestPoint = {0.0, 245.0, 0.0};
    deepGrounder.groundRollDistance = 207.0;
    for (int y = 38; y <= 245; y += 12) {
        deepGrounder.bounceTrajectory.push_back({0.0, static_cast<double>(y), 0.0});
    }

    joji::Random groundRandom{std::optional<std::uint32_t>{123}};
    const joji::PlayResolution ground = resolver.resolve(deepGrounder, state, groundRandom);
    assert(ground.fielderId != 0); // pitcher should not chase deep grounders into the outfield
    if (ground.fielderId >= 0 && ground.fieldingOutcome == joji::FieldingOutcomeType::FieldedCleanly) {
        assert(ground.fieldingPoint.y <= 175.0);
    }

    joji::BattedBall foulFly;
    foulFly.exitVelocity = 86.0;
    foulFly.launchAngle = 52.0;
    foulFly.sprayAngle = -48.0;
    foulFly.estimatedDistance = 92.0;
    foulFly.hangTime = 3.1;
    foulFly.maxHeight = 78.0;
    foulFly.landsFair = false;
    foulFly.classification = "pop fly";
    foulFly.landingPoint = {-86.0, 34.0, 0.0};
    foulFly.finalRestPoint = foulFly.landingPoint;
    foulFly.trajectory = {{0.0, 0.0, 1.0}, {-42.0, 17.0, 62.0}, foulFly.landingPoint};

    joji::Random foulRandom{std::optional<std::uint32_t>{456}};
    const joji::PlayResolution foul = resolver.resolve(foulFly, state, foulRandom);
    assert(foul.type == joji::PlayOutcomeType::Out);
    assert(foul.fieldingOutcome == joji::FieldingOutcomeType::Caught);
    assert(foul.fielderId >= 0);
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
    assert(!plan.replayTimeline.events.empty());
    bool hasPitchEvent = false;
    bool hasContactEvent = false;
    bool hasBallFlightEvent = false;
    bool hasFieldEvent = false;
    bool hasResultEvent = false;
    for (const auto& event : plan.replayTimeline.events) {
        hasPitchEvent = hasPitchEvent || event.type == joji::ReplayEventType::Pitch;
        hasContactEvent = hasContactEvent || event.type == joji::ReplayEventType::Contact;
        hasBallFlightEvent = hasBallFlightEvent || event.type == joji::ReplayEventType::BallFlight;
        hasFieldEvent = hasFieldEvent || event.type == joji::ReplayEventType::Field;
        hasResultEvent = hasResultEvent || event.type == joji::ReplayEventType::Result;
        assert(event.timeSeconds >= 0.0);
    }
    assert(hasPitchEvent);
    assert(hasContactEvent);
    assert(hasBallFlightEvent);
    assert(hasFieldEvent);
    assert(hasResultEvent);

    const joji::ReplayCursor startCursor =
        joji::cursorForReplayTimeline(plan.replayTimeline, 0.0);
    assert(startCursor.activeEventIndex.has_value() || startCursor.nextEventIndex.has_value());
    assert(startCursor.phase == joji::ReplayPhase::Pitch);

    const joji::ReplayCursor endCursor =
        joji::cursorForReplayTimeline(plan.replayTimeline, plan.totalDurationSeconds + 1.0);
    assert(endCursor.activeEventIndex.has_value());
    assert(endCursor.phase == joji::ReplayPhase::Result);

    const std::string json = joji::exportAnimationPlanToJson(plan);
    assert(json.find("\"schemaName\": \"joji.replay.timeline\"") != std::string::npos);
    assert(json.find("\"pitch\"") != std::string::npos);
    assert(json.find("\"battedBall\"") != std::string::npos);
    assert(json.find("\"runners\"") != std::string::npos);
    assert(json.find("\"defenders\"") != std::string::npos);
    assert(json.find("\"throws\"") != std::string::npos);
    assert(json.find("\"runnerMovements\"") != std::string::npos);
    assert(json.find("\"throwMovements\"") != std::string::npos);
    assert(json.find("\"tagPlays\"") != std::string::npos);
    assert(json.find("\"defensiveDecision\"") != std::string::npos);
}

} // namespace

int main() {
    testCountRules();
    testAtBatTerminates();
    testBallPhysicsAndResolution();
    testFieldingPhysicsBoundaries();
    testAnimationPlanBuilder();

    std::cout << "engine_tests: ok\n";
    return 0;
}
