#include "AtBatEngine.h"
#include "AnimationPlanBuilder.h"
#include "BallPhysicsEngine.h"
#include "JsonExporter.h"
#include "PlayResolutionEngine.h"
#include "Random.h"
#include "RunExpectancy.h"
#include "test_support.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace {

joji::Player createBatter() {
    joji::Player player;
    player.name = "Test Batter";
    player.position = joji::Position::CenterField;
    player.contact = 72;
    player.power = 64;
    player.eye = 68;
    player.speed = 74;
    player.fielding = 61;
    player.arm = 55;
    player.pitchingVelocity = 35;
    player.pitchingControl = 34;
    player.pitchingStuff = 32;
    return player;
}

joji::Player createPitcher() {
    joji::Player player;
    player.name = "Test Pitcher";
    player.position = joji::Position::Pitcher;
    player.contact = 41;
    player.power = 48;
    player.eye = 45;
    player.speed = 38;
    player.fielding = 50;
    player.arm = 64;
    player.pitchingVelocity = 82;
    player.pitchingControl = 61;
    player.pitchingStuff = 77;
    return player;
}

void testCountRules() {
    CHECK(!joji::isWalk({3, 0}));
    CHECK(joji::isWalk({4, 0}));
    CHECK(!joji::isStrikeOut({0, 2}));
    CHECK(joji::isStrikeOut({0, 3}));

    joji::Count count{0, 2};
    count = joji::afterFoul(count);
    CHECK(count.balls == 0);
    CHECK(count.strikes == 2);

    count = joji::afterBall({3, 1});
    CHECK(count.balls == 4);
    CHECK(count.strikes == 1);
}

void testChanceFromRoll() {
    CHECK(!joji::chanceFromRoll(0.0, 0.0));
    CHECK(!joji::chanceFromRoll(-1.0, -10.0));
    CHECK(joji::chanceFromRoll(1.0, 0.999));
    CHECK(joji::chanceFromRoll(1.5, 100.0));
    CHECK(joji::chanceFromRoll(0.25, 0.249));
    CHECK(!joji::chanceFromRoll(0.25, 0.25));
    CHECK(!joji::chanceFromRoll(0.25, 0.90));
}

void testRunExpectancyTable() {
    CHECK(joji::runExpectancy(0, true, false, false)
          > joji::runExpectancy(0, false, false, false));
    CHECK(joji::runExpectancy(0, false, true, false)
          > joji::runExpectancy(1, false, true, false));

    joji::GameState state;
    state.outs = 0;
    state.bases[0] = "Runner";
    const joji::RunExpectancyDelta steal =
        joji::stealRunExpectancyDelta(state, 1, 2);
    CHECK(steal.success > steal.current);
    CHECK(steal.failure < steal.current);
    CHECK(steal.breakEvenSuccessRate > 0.60);
    CHECK(steal.breakEvenSuccessRate < 0.80);
}

void testAtBatTerminates() {
    joji::Random random{std::optional<std::uint32_t>{42}};
    joji::AtBatEngine engine;
    const joji::AtBatResult result = engine.simulatePlateAppearance(createBatter(), createPitcher(), random);

    CHECK(!result.pitchLogs.empty());
    CHECK(result.finalOutcome == joji::AtBatOutcome::Walk ||
           result.finalOutcome == joji::AtBatOutcome::StrikeOut ||
           result.finalOutcome == joji::AtBatOutcome::InPlay ||
           result.finalOutcome == joji::AtBatOutcome::HitByPitch);

    if (result.finalOutcome == joji::AtBatOutcome::InPlay) {
        CHECK(result.battedBallInput.has_value());
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
    CHECK(ball.estimatedDistance > 250.0);
    CHECK(!ball.trajectory.empty());
    CHECK(!ball.bounceTrajectory.empty());
    CHECK(ball.hangTime > 0.0);
    CHECK(ball.maxHeight > 0.0);
    CHECK(ball.finalRestPoint.y >= ball.landingPoint.y);

    const joji::PlayResolution resolution = resolver.resolve(ball, state, random);
    CHECK(resolution.type == joji::PlayOutcomeType::Out ||
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
    CHECK(ground.fielderId != 0); // pitcher should not chase deep grounders into the outfield
    if (ground.fielderId >= 0 && ground.fieldingOutcome == joji::FieldingOutcomeType::FieldedCleanly) {
        CHECK(ground.fieldingPoint.y <= 175.0);
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
    CHECK(foul.type == joji::PlayOutcomeType::Out);
    CHECK(foul.fieldingOutcome == joji::FieldingOutcomeType::Caught);
    CHECK(foul.fielderId >= 0);
}

void testDeterministicFieldingBoundaries() {
    joji::PlayResolutionEngine resolver;
    joji::GameState state;
    joji::Random random{std::optional<std::uint32_t>{999}};

    joji::BattedBall foulGrounder;
    foulGrounder.exitVelocity = 55.0;
    foulGrounder.launchAngle = -12.0;
    foulGrounder.sprayAngle = -55.0;
    foulGrounder.estimatedDistance = 35.0;
    foulGrounder.hangTime = 0.2;
    foulGrounder.maxHeight = 1.0;
    foulGrounder.landsFair = false;
    foulGrounder.classification = "ground ball";
    foulGrounder.landingPoint = {-42.0, 8.0, 0.0};
    foulGrounder.finalRestPoint = {-55.0, 12.0, 0.0};

    const joji::PlayResolution foul = resolver.resolve(foulGrounder, state, random);
    CHECK(foul.type == joji::PlayOutcomeType::Foul);
    CHECK(foul.outsRecorded == 0);
    CHECK(foul.fielderId == -1);

    joji::BattedBall homeRun;
    homeRun.exitVelocity = 108.0;
    homeRun.launchAngle = 30.0;
    homeRun.sprayAngle = 4.0;
    homeRun.estimatedDistance = 410.0;
    homeRun.hangTime = 4.8;
    homeRun.maxHeight = 95.0;
    homeRun.landsFair = true;
    homeRun.crossesFence = true;
    homeRun.fenceCrossHeight = 18.0;
    homeRun.classification = "fly ball";
    homeRun.landingPoint = {20.0, 410.0, 0.0};
    homeRun.finalRestPoint = homeRun.landingPoint;

    const joji::PlayResolution homer = resolver.resolve(homeRun, state, random);
    CHECK(homer.type == joji::PlayOutcomeType::HomeRun);
    CHECK(homer.basesAwarded == 4);
    CHECK(homer.outsRecorded == 0);
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

    CHECK(plan.gameId == "test-game");
    CHECK(plan.hasPitch);
    CHECK(plan.pitch.batterName == atBat.batter.name);
    CHECK(plan.pitch.pitcherName == atBat.pitcher.name);
    CHECK(plan.pitch.isInPlay);
    CHECK(plan.pitch.durationSeconds > 0.0);
    CHECK(!plan.pitch.points.empty());

    CHECK(plan.hasBattedBall);
    CHECK(plan.battedBall.result == "single");
    CHECK(plan.battedBall.durationSeconds >= ball.hangTime);
    CHECK(plan.battedBall.estimatedDistance == ball.estimatedDistance);
    CHECK(!plan.battedBall.points.empty());
    CHECK(plan.runners.empty());
    CHECK(plan.totalDurationSeconds >= plan.pitch.durationSeconds);
    CHECK(plan.totalDurationSeconds >= plan.battedBall.durationSeconds);
    CHECK(!plan.replayTimeline.events.empty());
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
        CHECK(event.timeSeconds >= 0.0);
    }
    CHECK(hasPitchEvent);
    CHECK(hasContactEvent);
    CHECK(hasBallFlightEvent);
    CHECK(hasFieldEvent);
    CHECK(hasResultEvent);

    const joji::ReplayCursor startCursor =
        joji::cursorForReplayTimeline(plan.replayTimeline, 0.0);
    CHECK(startCursor.activeEventIndex.has_value() || startCursor.nextEventIndex.has_value());
    CHECK(startCursor.phase == joji::ReplayPhase::Pitch);

    const joji::ReplayCursor endCursor =
        joji::cursorForReplayTimeline(plan.replayTimeline, plan.totalDurationSeconds + 1.0);
    CHECK(endCursor.activeEventIndex.has_value());
    CHECK(endCursor.phase == joji::ReplayPhase::Result);

    const std::string json = joji::exportAnimationPlanToJson(plan);
    CHECK(json.find("\"schemaName\": \"joji.replay.timeline\"") != std::string::npos);
    CHECK(json.find("\"pitch\"") != std::string::npos);
    CHECK(json.find("\"battedBall\"") != std::string::npos);
    CHECK(json.find("\"runners\"") != std::string::npos);
    CHECK(json.find("\"defenders\"") != std::string::npos);
    CHECK(json.find("\"throws\"") != std::string::npos);
    CHECK(json.find("\"runnerMovements\"") != std::string::npos);
    CHECK(json.find("\"throwMovements\"") != std::string::npos);
    CHECK(json.find("\"tagPlays\"") != std::string::npos);
    CHECK(json.find("\"defensiveDecision\"") != std::string::npos);
    CHECK(joji_tests::isValidJson(json));
}

} // namespace

int main() {
    testCountRules();
    testChanceFromRoll();
    testRunExpectancyTable();
    testAtBatTerminates();
    testBallPhysicsAndResolution();
    testFieldingPhysicsBoundaries();
    testDeterministicFieldingBoundaries();
    testAnimationPlanBuilder();
    joji_tests::testJsonSyntaxChecker();

    if (joji_tests::failures > 0) {
        std::cerr << "engine_tests: " << joji_tests::failures << " failure(s)\n";
        return 1;
    }

    std::cout << "engine_tests: ok\n";
    return 0;
}
