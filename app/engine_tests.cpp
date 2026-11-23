#include "AtBatEngine.h"
#include "AnimationPlanBuilder.h"
#include "BallPhysicsEngine.h"
#include "DraftEngine.h"
#include "GameEngine.h"
#include "JsonExporter.h"
#include "PlayResolutionEngine.h"
#include "ProgressionEngine.h"
#include "Random.h"
#include "RunExpectancy.h"
#include "Teams.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
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

// ── Draft engine ──────────────────────────────────────────────────────────────

std::string draftFingerprint(const joji::DraftResult& result) {
    std::string out;
    for (const auto& pick : result.picks) {
        out += std::to_string(pick.overall) + ":" + pick.team + ":" + std::to_string(pick.prospectId) + ";";
    }
    for (const auto& change : result.changes) {
        out += change.team + "|" + change.player + "|" + change.replaced + "|" + std::to_string(change.overall) + ";";
    }
    for (const auto& prospect : result.pool) {
        out += prospect.name + std::to_string(prospect.overall) + ";";
    }
    return out;
}

void testDraftDeterministic() {
    const auto teams = joji::allTeams();
    joji::DraftConfig config;
    config.seed = 7;

    joji::DraftResult first = joji::runDraft(teams, config);
    joji::applyDraftToRosters(teams, first, config);
    joji::DraftResult second = joji::runDraft(teams, config);
    joji::applyDraftToRosters(teams, second, config);
    CHECK(draftFingerprint(first) == draftFingerprint(second));

    joji::DraftConfig other = config;
    other.seed = 8;
    joji::DraftResult third = joji::runDraft(teams, other);
    joji::applyDraftToRosters(teams, third, other);
    CHECK(draftFingerprint(first) != draftFingerprint(third));
}

void testDraftPicksAreComplete() {
    const auto teams = joji::allTeams();
    joji::DraftConfig config;
    const joji::DraftResult result = joji::runDraft(teams, config);

    CHECK(result.picks.size() == teams.size() * static_cast<std::size_t>(config.rounds));
    CHECK(result.pool.size() >= result.picks.size());

    std::set<int> prospects;
    std::map<std::string, int> perTeam;
    for (const auto& pick : result.picks) {
        prospects.insert(pick.prospectId);
        ++perTeam[pick.team];
    }
    CHECK(prospects.size() == result.picks.size());  // nobody drafted twice
    CHECK(perTeam.size() == teams.size());
    for (const auto& entry : perTeam) CHECK(entry.second == config.rounds);

    // Snake: the team that picks last in round 1 picks first in round 2.
    const std::size_t n = teams.size();
    CHECK(result.picks[n - 1].team == result.picks[n].team);
    CHECK(result.picks[0].team == result.picks[2 * n - 1].team);
}

void testProspectPoolIsSane() {
    const auto teams = joji::allTeams();
    std::set<std::string> existing;
    for (const auto& team : teams) {
        for (const auto& p : team.lineup()) existing.insert(p.name);
        for (const auto& p : team.rotation()) existing.insert(p.name);
        for (const auto& p : team.bullpen()) existing.insert(p.name);
        for (const auto& p : team.bench()) existing.insert(p.name);
    }

    joji::DraftConfig config;
    const joji::DraftResult result = joji::runDraft(teams, config);

    std::set<std::string> names;
    int pitchers = 0;
    for (const auto& prospect : result.pool) {
        const joji::Player& p = prospect.asPlayer;
        CHECK(names.insert(prospect.name).second);              // unique within the pool
        CHECK(existing.count(prospect.name) == 0);               // never clashes with a current player
        CHECK(prospect.age >= 18 && prospect.age <= 22);
        CHECK(prospect.potential >= prospect.readiness);
        const bool isPitcher = prospect.position == joji::Position::Pitcher;
        if (isPitcher) {
            ++pitchers;
            CHECK(!p.arsenal.empty());
            CHECK(p.pitchingVelocity >= 30 && p.pitchingVelocity <= 95);
            CHECK(p.pitchingStamina >= 30 && p.pitchingStamina <= 95);
        } else {
            CHECK(p.contact >= 30 && p.contact <= 95);
            CHECK(p.power >= 30 && p.power <= 95);
        }
    }
    CHECK(pitchers > 0 && pitchers < static_cast<int>(result.pool.size()));
}

void testDraftKeepsRosterShape() {
    const auto teams = joji::allTeams();
    joji::DraftConfig config;
    config.seed = 3;
    joji::DraftResult result = joji::runDraft(teams, config);
    const auto updated = joji::applyDraftToRosters(teams, result, config);

    CHECK(updated.size() == teams.size());
    for (std::size_t i = 0; i < teams.size(); ++i) {
        CHECK(updated[i].name() == teams[i].name());
        CHECK(updated[i].lineup().size() == teams[i].lineup().size());
        CHECK(updated[i].rotation().size() == teams[i].rotation().size());
        CHECK(updated[i].bullpen().size() == teams[i].bullpen().size());
        CHECK(updated[i].bench().size() == teams[i].bench().size());
        CHECK(updated[i].league() == teams[i].league());
        // Starters never lose their jobs in v1.
        for (std::size_t j = 0; j < teams[i].lineup().size(); ++j) {
            CHECK(updated[i].lineup()[j].name == teams[i].lineup()[j].name);
        }
        for (std::size_t j = 0; j < teams[i].rotation().size(); ++j) {
            CHECK(updated[i].rotation()[j].name == teams[i].rotation()[j].name);
        }
    }

    int promoted = 0;
    int released = 0;
    int minors = 0;
    for (const auto& change : result.changes) {
        if (change.type == joji::RosterChangeType::Promoted) ++promoted;
        else if (change.type == joji::RosterChangeType::Released) ++released;
        else ++minors;
    }
    CHECK(promoted == released);                          // every promotion displaces exactly one player
    CHECK(promoted + minors == static_cast<int>(result.picks.size()));  // every pick is accounted for
}

void testDraftPromotionRateIsReasonable() {
    const auto teams = joji::allTeams();
    int promoted = 0;
    const int seeds = 40;
    for (int seed = 1; seed <= seeds; ++seed) {
        joji::DraftConfig config;
        config.seed = static_cast<unsigned>(seed);
        joji::DraftResult result = joji::runDraft(teams, config);
        joji::applyDraftToRosters(teams, result, config);
        for (const auto& change : result.changes) {
            if (change.type == joji::RosterChangeType::Promoted) ++promoted;
        }
    }
    const double perTeam = static_cast<double>(promoted) / seeds / static_cast<double>(teams.size());
    // Default margin targets ~1.8 roster moves per team; guard against drift to "nobody" or "everybody".
    CHECK(perTeam > 0.8 && perTeam < 3.0);
}

void testDraftStrategyIsPluggable() {
    const auto teams = joji::allTeams();
    joji::DraftConfig config;
    config.seed = 5;
    // Always take the best remaining prospect: the very first pick must be the pool's best.
    const joji::DraftStrategy best = [](const std::vector<joji::DraftProspect>& available,
                                        const joji::Team&, joji::Random&) {
        std::size_t top = 0;
        for (std::size_t i = 1; i < available.size(); ++i) {
            if (available[i].overall > available[top].overall) top = i;
        }
        return top;
    };
    const joji::DraftResult result = joji::runDraft(teams, config, best);
    int bestOverall = 0;
    int firstPickId = -1;
    for (const auto& prospect : result.pool) bestOverall = std::max(bestOverall, prospect.overall);
    for (const auto& prospect : result.pool) {
        if (prospect.id == result.picks[0].prospectId) firstPickId = prospect.overall;
    }
    CHECK(firstPickId == bestOverall);
}

// Every game has exactly one winning and one losing pitcher (on the right teams), and the
// winning pitcher never also gets the save.
void testPitcherDecisions() {
    const auto teams = joji::allTeams();
    int starterLosses = 0;
    for (unsigned game = 0; game < 60; ++game) {
        const auto& away = teams[game % teams.size()];
        const auto& home = teams[(game * 7 + 3) % teams.size()];
        if (away.name() == home.name()) continue;

        joji::GameEngine engine(away, home, joji::Random{std::optional<std::uint32_t>{5000u + game}},
                                joji::GameRules{}, home.homeBallpark());
        engine.setFastMode(true);
        std::ostringstream sink;
        engine.simulate(sink);

        const bool awayWon = engine.state().awayScore > engine.state().homeScore;
        const auto& winStats = awayWon ? engine.awayPitcherStats() : engine.homePitcherStats();
        const auto& loseStats = awayWon ? engine.homePitcherStats() : engine.awayPitcherStats();
        int wins = 0, losses = 0;
        std::string winner, saver;
        for (const auto& p : winStats) {
            wins += p.wins;
            if (p.wins) winner = p.name;
            if (p.saves) saver = p.name;
            CHECK(p.losses == 0);
        }
        for (const auto& p : loseStats) {
            losses += p.losses;
            CHECK(p.wins == 0 && p.saves == 0);
            if (p.losses && p.gamesStarted > 0) ++starterLosses;
        }
        CHECK(wins == 1);
        CHECK(losses == 1);
        CHECK(saver.empty() || saver != winner);
    }
    // Before the fix a starter could never take a loss.
    CHECK(starterLosses > 0);
}

// ── Progression (aging & rookie development) ──────────────────────────────────

std::string rosterFingerprint(const std::vector<joji::Team>& teams) {
    std::string out;
    for (const auto& team : teams) {
        auto add = [&](const std::vector<joji::Player>& group) {
            for (const auto& p : group) {
                out += p.name + ":" + std::to_string(p.age) + ":" + std::to_string(p.contact) + ":" +
                       std::to_string(p.power) + ":" + std::to_string(p.pitchingVelocity) + ":" +
                       std::to_string(p.pitchingStuff) + ":" + std::to_string(p.pitchingStamina) + ";";
            }
        };
        add(team.lineup());
        add(team.rotation());
        add(team.bullpen());
        add(team.bench());
        out += "|";
    }
    return out;
}

void testProgressionDeterministicAndShaped() {
    const auto teams = joji::allTeams();
    joji::ProgressionConfig config;
    config.seed = 11;
    joji::ProgressionResult first, second, other;
    const auto a = joji::advanceOneYear(teams, config, first);
    const auto b = joji::advanceOneYear(teams, config, second);
    CHECK(rosterFingerprint(a) == rosterFingerprint(b));
    config.seed = 12;
    const auto c = joji::advanceOneYear(teams, config, other);
    CHECK(rosterFingerprint(a) != rosterFingerprint(c));

    // Same sizes, everyone a year older, ratings in range.
    for (std::size_t t = 0; t < teams.size(); ++t) {
        CHECK(a[t].lineup().size() == teams[t].lineup().size());
        CHECK(a[t].rotation().size() == teams[t].rotation().size());
        CHECK(a[t].bullpen().size() == teams[t].bullpen().size());
        CHECK(a[t].bench().size() == teams[t].bench().size());
        for (std::size_t i = 0; i < teams[t].lineup().size(); ++i) {
            CHECK(a[t].lineup()[i].age == teams[t].lineup()[i].age + 1);
            CHECK(a[t].lineup()[i].contact >= 30 && a[t].lineup()[i].contact <= 95);
        }
        for (std::size_t i = 0; i < teams[t].rotation().size(); ++i) {
            CHECK(a[t].rotation()[i].pitchingStuff >= 30 && a[t].rotation()[i].pitchingStuff <= 95);
        }
    }

    // Players in `skip` are copied unchanged.
    std::set<std::string> skip{teams[0].lineup()[0].name};
    joji::ProgressionResult skipped;
    const auto d = joji::advanceOneYear(teams, config, skipped, skip);
    CHECK(d[0].lineup()[0].age == teams[0].lineup()[0].age);
    CHECK(d[0].lineup()[0].contact == teams[0].lineup()[0].contact);
}

void testProgressionAgeCurve() {
    // Over several years the young improve and the old decline, on average.
    std::vector<joji::Team> teams = joji::allTeams();
    double youngBefore = 0, youngAfter = 0, oldBefore = 0, oldAfter = 0;
    int young = 0, old = 0;
    joji::ProgressionConfig config;
    for (int year = 0; year < 3; ++year) {
        config.seed = 100u + static_cast<unsigned>(year);
        joji::ProgressionResult result;
        teams = joji::advanceOneYear(teams, config, result);
        for (const auto& change : result.changes) {
            if (change.ageBefore <= 23) { youngBefore += change.overallBefore; youngAfter += change.overallAfter; ++young; }
            if (change.ageBefore >= 34) { oldBefore += change.overallBefore; oldAfter += change.overallAfter; ++old; }
        }
    }
    CHECK(young > 10 && old > 10);
    CHECK(youngAfter / young > youngBefore / young);
    CHECK(oldAfter / old < oldBefore / old);
}

void testRookiesDevelopTowardPotential() {
    // Rookies from a draft grow toward their ceiling and stop at it.
    const auto base = joji::allTeams();
    joji::DraftConfig draftConfig;
    draftConfig.seed = 3;
    joji::DraftResult draft = joji::runDraft(base, draftConfig);
    std::vector<joji::Team> teams = joji::applyDraftToRosters(base, draft, draftConfig);

    std::map<std::string, std::pair<int, int>> developing;  // name -> {potential, readiness}
    for (const auto& team : teams) {
        for (const auto* group : {&team.bullpen(), &team.bench()}) {
            for (const auto& p : *group) {
                if (p.potential > p.readiness) developing[p.name] = {p.potential, p.readiness};
            }
        }
    }
    CHECK(!developing.empty());

    double overallBefore = 0;
    for (const auto& team : teams) {
        for (const auto* group : {&team.bullpen(), &team.bench()}) {
            for (const auto& p : *group) if (developing.count(p.name)) overallBefore += joji::playerOverall(p);
        }
    }
    joji::ProgressionConfig config;
    for (int year = 0; year < 4; ++year) {
        config.seed = 7u + static_cast<unsigned>(year);
        joji::ProgressionResult result;
        teams = joji::advanceOneYear(teams, config, result);
    }
    double overallAfter = 0;
    for (const auto& team : teams) {
        for (const auto* group : {&team.bullpen(), &team.bench()}) {
            for (const auto& p : *group) {
                const auto it = developing.find(p.name);
                if (it == developing.end()) continue;
                overallAfter += joji::playerOverall(p);
                CHECK(p.readiness >= it->second.second);
                CHECK(p.readiness <= it->second.first);  // never past the ceiling
            }
        }
    }
    CHECK(overallAfter > overallBefore);
}

void testSeasonRostersFollowTheDraft() {
    const auto base = joji::allTeams();
    // 2026 is the hand-written roster, untouched.
    const auto s2026 = joji::buildSeasonRosters(base, 2026);
    CHECK(rosterFingerprint(s2026.teams) == rosterFingerprint(base));
    CHECK(s2026.draft.picks.empty());

    // The first offseason drafts from the 2026 rosters, so the draft equals a plain draft.
    const auto s2027 = joji::buildSeasonRosters(base, 2027);
    joji::DraftConfig config;
    config.season = 2027;
    config.seed = 2027;
    const joji::DraftResult plain = joji::runDraft(base, config);
    CHECK(s2027.draft.picks.size() == plain.picks.size());
    bool same = true;
    for (std::size_t i = 0; i < plain.picks.size(); ++i) {
        same = same && s2027.draft.picks[i].prospectId == plain.picks[i].prospectId &&
               s2027.draft.picks[i].team == plain.picks[i].team;
    }
    CHECK(same);
    CHECK(rosterFingerprint(s2027.teams) != rosterFingerprint(base));

    // Rebuilding gives the same rosters.
    CHECK(rosterFingerprint(joji::buildSeasonRosters(base, 2029).teams) ==
          rosterFingerprint(joji::buildSeasonRosters(base, 2029).teams));
}

void testRetirements() {
    CHECK(joji::retirementChance(30, 60) == 0.0);
    CHECK(joji::retirementChance(36, 60) < joji::retirementChance(39, 60));
    CHECK(joji::retirementChance(37, 75) < joji::retirementChance(37, 50));  // stars stay longer
    CHECK(joji::retirementChance(42, 90) >= 0.99 || joji::retirementChance(42, 90) > 0.5);

    const auto base = joji::allTeams();
    const auto rosters = joji::buildSeasonRosters(base, 2034);
    CHECK(!rosters.progression.retirements.empty());
    for (const auto& team : rosters.teams) {
        CHECK(team.lineup().size() == 9 && team.rotation().size() == 5);
        CHECK(team.bullpen().size() == 8 && team.bench().size() == 5);
    }
    // A retiree is gone from his team and his replacement is on it.
    for (const auto& r : rosters.progression.retirements) {
        for (const auto& team : rosters.teams) {
            if (team.name() != r.team) continue;
            bool retireeThere = false, replacementThere = false;
            for (const auto* group : {&team.lineup(), &team.rotation(), &team.bullpen(), &team.bench()}) {
                for (const auto& p : *group) {
                    retireeThere = retireeThere || p.name == r.player;
                    replacementThere = replacementThere || p.name == r.replacement;
                }
            }
            CHECK(!retireeThere);
            CHECK(replacementThere);
        }
    }
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
    testDraftDeterministic();
    testDraftPicksAreComplete();
    testProspectPoolIsSane();
    testDraftKeepsRosterShape();
    testDraftPromotionRateIsReasonable();
    testDraftStrategyIsPluggable();
    testPitcherDecisions();
    testProgressionDeterministicAndShaped();
    testProgressionAgeCurve();
    testRetirements();
    testRookiesDevelopTowardPotential();
    testSeasonRostersFollowTheDraft();
    joji_tests::testJsonSyntaxChecker();

    if (joji_tests::failures > 0) {
        std::cerr << "engine_tests: " << joji_tests::failures << " failure(s)\n";
        return 1;
    }

    std::cout << "engine_tests: ok\n";
    return 0;
}
