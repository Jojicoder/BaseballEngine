#include "JsonExporter.h"
#include "AnimationTypes.h"
#include "GameEngine.h"

#include <iomanip>
#include <sstream>

namespace joji {

namespace {

std::string q(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    out += "\"";
    return out;
}

std::string dbl(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

std::string intArray(const std::vector<int>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) out += ",";
        out += (v[i] < 0 ? "null" : std::to_string(v[i]));
    }
    return out + "]";
}

std::string resultTypeName(GameResultType t) {
    switch (t) {
        case GameResultType::AwayWin: return "AwayWin";
        case GameResultType::HomeWin: return "HomeWin";
        case GameResultType::Tie:     return "Tie";
    }
    return "Unknown";
}

std::string replayEventTypeName(ReplayEventType type) {
    switch (type) {
        case ReplayEventType::Pitch:      return "Pitch";
        case ReplayEventType::Contact:    return "Contact";
        case ReplayEventType::BallFlight: return "BallFlight";
        case ReplayEventType::Field:      return "Field";
        case ReplayEventType::Throw:      return "Throw";
        case ReplayEventType::Runner:     return "Runner";
        case ReplayEventType::Tag:        return "Tag";
        case ReplayEventType::Result:     return "Result";
    }
    return "Result";
}

std::string replayPhaseName(ReplayPhase phase) {
    switch (phase) {
        case ReplayPhase::Pitch:      return "PitchPhase";
        case ReplayPhase::Contact:    return "ContactPhase";
        case ReplayPhase::BallFlight: return "BallFlightPhase";
        case ReplayPhase::Field:      return "FieldPhase";
        case ReplayPhase::Throw:      return "ThrowPhase";
        case ReplayPhase::Runner:     return "RunnerPhase";
        case ReplayPhase::Tag:        return "TagPhase";
        case ReplayPhase::Result:     return "ResultPhase";
    }
    return "ResultPhase";
}

std::string pointJson(const AnimationPoint& p, const std::string& indent) {
    std::string s = indent + "{";
    s += q("x") + ": " + dbl(p.x) + ", ";
    s += q("y") + ": " + dbl(p.y) + ", ";
    s += q("z") + ": " + dbl(p.z) + ", ";
    s += q("t") + ": " + dbl(p.timeSeconds);
    s += "}";
    return s;
}

std::string pointArrayJson(const std::vector<AnimationPoint>& points,
                           const std::string& indent) {
    std::string s = "[\n";
    for (std::size_t i = 0; i < points.size(); ++i) {
        s += pointJson(points[i], indent + "  ");
        if (i + 1 < points.size()) s += ",";
        s += "\n";
    }
    s += indent + "]";
    return s;
}

std::string pitchAnimationJson(const PitchAnimation& pitch,
                               const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("pitcherName") + ": " + q(pitch.pitcherName) + ",\n";
    s += indent + "  " + q("batterName") + ": " + q(pitch.batterName) + ",\n";
    s += indent + "  " + q("pitchType") + ": " + q(pitch.pitchType) + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(pitch.durationSeconds) + ",\n";
    s += indent + "  " + q("velocity") + ": " + dbl(pitch.velocity) + ",\n";
    s += indent + "  " + q("movementX") + ": " + dbl(pitch.movementX) + ",\n";
    s += indent + "  " + q("movementZ") + ": " + dbl(pitch.movementZ) + ",\n";
    s += indent + "  " + q("endLocationX") + ": " + dbl(pitch.endLocationX) + ",\n";
    s += indent + "  " + q("endLocationZ") + ": " + dbl(pitch.endLocationZ) + ",\n";
    s += indent + "  " + q("zoneNumber") + ": " + std::to_string(pitch.zoneNumber) + ",\n";
    s += indent + "  " + q("result") + ": " + q(pitch.result) + ",\n";
    s += indent + "  " + q("isStrike") + ": " + std::string(pitch.isStrike ? "true" : "false") + ",\n";
    s += indent + "  " + q("isBall") + ": " + std::string(pitch.isBall ? "true" : "false") + ",\n";
    s += indent + "  " + q("isInPlay") + ": " + std::string(pitch.isInPlay ? "true" : "false") + ",\n";
    s += indent + "  " + q("start") + ": " + pointJson(pitch.start, "") + ",\n";
    s += indent + "  " + q("end") + ": " + pointJson(pitch.end, "") + ",\n";
    s += indent + "  " + q("points") + ": " + pointArrayJson(pitch.points, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string battedBallAnimationJson(const BattedBallAnimation& ball,
                                    const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("batterName") + ": " + q(ball.batterName) + ",\n";
    s += indent + "  " + q("result") + ": " + q(ball.result) + ",\n";
    s += indent + "  " + q("classification") + ": " + q(ball.classification) + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(ball.durationSeconds) + ",\n";
    s += indent + "  " + q("estimatedDistance") + ": " + dbl(ball.estimatedDistance) + ",\n";
    s += indent + "  " + q("maxHeight") + ": " + dbl(ball.maxHeight) + ",\n";
    s += indent + "  " + q("landsFair") + ": " + std::string(ball.landsFair ? "true" : "false") + ",\n";
    s += indent + "  " + q("crossesFence") + ": " + std::string(ball.crossesFence ? "true" : "false") + ",\n";
    s += indent + "  " + q("landingPoint") + ": " + pointJson(ball.landingPoint, "") + ",\n";
    s += indent + "  " + q("finalPoint") + ": " + pointJson(ball.finalPoint, "") + ",\n";
    s += indent + "  " + q("points") + ": " + pointArrayJson(ball.points, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string runnerAnimationJson(const RunnerAnimation& runner,
                                const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("runnerName") + ": " + q(runner.runnerName) + ",\n";
    s += indent + "  " + q("fromBase") + ": " + std::to_string(runner.fromBase) + ",\n";
    s += indent + "  " + q("toBase") + ": " + std::to_string(runner.toBase) + ",\n";
    s += indent + "  " + q("scored") + ": " + std::string(runner.scored ? "true" : "false") + ",\n";
    s += indent + "  " + q("out") + ": " + std::string(runner.out ? "true" : "false") + ",\n";
    s += indent + "  " + q("outAtBase") + ": " + q(runner.outAtBase) + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(runner.durationSeconds) + ",\n";
    s += indent + "  " + q("points") + ": " + pointArrayJson(runner.points, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string defenseAnimationJson(const DefenseAnimation& defender,
                                 const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("fielderId") + ": " + std::to_string(defender.fielderId) + ",\n";
    s += indent + "  " + q("fielderName") + ": " + q(defender.fielderName) + ",\n";
    s += indent + "  " + q("madePlay") + ": " + std::string(defender.madePlay ? "true" : "false") + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(defender.durationSeconds) + ",\n";
    s += indent + "  " + q("points") + ": " + pointArrayJson(defender.points, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string throwAnimationJson(const ThrowAnimation& throwAnimation,
                               const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("fielderId") + ": " + std::to_string(throwAnimation.fielderId) + ",\n";
    s += indent + "  " + q("fielderName") + ": " + q(throwAnimation.fielderName) + ",\n";
    s += indent + "  " + q("targetBase") + ": " + std::to_string(throwAnimation.targetBase) + ",\n";
    s += indent + "  " + q("startTimeOffset") + ": " + dbl(throwAnimation.startTimeOffset) + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(throwAnimation.durationSeconds) + ",\n";
    s += indent + "  " + q("badThrow") + ": " + std::string(throwAnimation.badThrow ? "true" : "false") + ",\n";
    s += indent + "  " + q("offlineFeet") + ": " + dbl(throwAnimation.offlineFeet) + ",\n";
    s += indent + "  " + q("points") + ": " + pointArrayJson(throwAnimation.points, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string runnerMovementJson(const RunnerMovement& runner,
                               const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("runnerName") + ": " + q(runner.runnerName) + ",\n";
    s += indent + "  " + q("fromBase") + ": " + std::to_string(runner.fromBase) + ",\n";
    s += indent + "  " + q("toBase") + ": " + std::to_string(runner.toBase) + ",\n";
    s += indent + "  " + q("startTime") + ": " + dbl(runner.startTime) + ",\n";
    s += indent + "  " + q("arrivalTime") + ": " + dbl(runner.arrivalTime) + ",\n";
    s += indent + "  " + q("topSpeedFeetPerSecond") + ": " + dbl(runner.topSpeedFeetPerSecond) + ",\n";
    s += indent + "  " + q("accelerationFeetPerSecond2") + ": " + dbl(runner.accelerationFeetPerSecond2) + ",\n";
    s += indent + "  " + q("turnPenaltySeconds") + ": " + dbl(runner.turnPenaltySeconds) + ",\n";
    s += indent + "  " + q("slideTimeSeconds") + ": " + dbl(runner.slideTimeSeconds) + ",\n";
    s += indent + "  " + q("usedForSafeOut") + ": " + std::string(runner.usedForSafeOut ? "true" : "false") + ",\n";
    s += indent + "  " + q("scored") + ": " + std::string(runner.scored ? "true" : "false") + ",\n";
    s += indent + "  " + q("out") + ": " + std::string(runner.out ? "true" : "false") + ",\n";
    s += indent + "  " + q("routePoints") + ": " + pointArrayJson(runner.routePoints, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string throwMovementJson(const ThrowMovement& throwMovement,
                              const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("fielderName") + ": " + q(throwMovement.fielderName) + ",\n";
    s += indent + "  " + q("targetBase") + ": " + std::to_string(throwMovement.targetBase) + ",\n";
    s += indent + "  " + q("startTime") + ": " + dbl(throwMovement.startTime) + ",\n";
    s += indent + "  " + q("arrivalTime") + ": " + dbl(throwMovement.arrivalTime) + ",\n";
    s += indent + "  " + q("velocityFeetPerSecond") + ": " + dbl(throwMovement.velocityFeetPerSecond) + ",\n";
    s += indent + "  " + q("accuracy") + ": " + dbl(throwMovement.accuracy) + ",\n";
    s += indent + "  " + q("fromPosition") + ": " + pointJson(throwMovement.fromPosition, "") + ",\n";
    s += indent + "  " + q("trajectory") + ": " + pointArrayJson(throwMovement.trajectory, indent + "  ") + "\n";
    s += indent + "}";
    return s;
}

std::string tagPlayJson(const TagPlay& tag,
                        const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("base") + ": " + std::to_string(tag.base) + ",\n";
    s += indent + "  " + q("runnerName") + ": " + q(tag.runnerName) + ",\n";
    s += indent + "  " + q("runnerArrivalTime") + ": " + dbl(tag.runnerArrivalTime) + ",\n";
    s += indent + "  " + q("ballArrivalTime") + ": " + dbl(tag.ballArrivalTime) + ",\n";
    s += indent + "  " + q("tagTime") + ": " + dbl(tag.tagTime) + ",\n";
    s += indent + "  " + q("runnerSafe") + ": " + std::string(tag.runnerSafe ? "true" : "false") + "\n";
    s += indent + "}";
    return s;
}

std::string replayDecisionJson(const ReplayDecision& decision,
                               const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("chosenTargetBase") + ": "
        + std::to_string(decision.chosenTargetBase) + ",\n";
    s += indent + "  " + q("holdBall") + ": "
        + std::string(decision.holdBall ? "true" : "false") + ",\n";
    s += indent + "  " + q("cutoff") + ": "
        + std::string(decision.cutoff ? "true" : "false") + ",\n";
    s += indent + "  " + q("reason") + ": " + q(decision.reason) + "\n";
    s += indent + "}";
    return s;
}

template <typename T, typename Formatter>
std::string objectArrayJson(const std::vector<T>& items,
                            const std::string& indent,
                            Formatter formatter) {
    std::string s = "[\n";
    for (std::size_t i = 0; i < items.size(); ++i) {
        s += formatter(items[i], indent + "  ");
        if (i + 1 < items.size()) s += ",";
        s += "\n";
    }
    s += indent + "]";
    return s;
}

std::string replayEventJson(const ReplayEvent& event, const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("type") + ": " + q(replayEventTypeName(event.type)) + ",\n";
    s += indent + "  " + q("phase") + ": " + q(replayPhaseName(phaseForReplayEvent(event.type))) + ",\n";
    s += indent + "  " + q("timeSeconds") + ": " + dbl(event.timeSeconds) + ",\n";
    s += indent + "  " + q("label") + ": " + q(event.label) + ",\n";
    s += indent + "  " + q("actor") + ": " + q(event.actor) + ",\n";
    s += indent + "  " + q("detail") + ": " + q(event.detail) + ",\n";
    s += indent + "  " + q("result") + ": " + q(event.result) + ",\n";
    s += indent + "  " + q("base") + ": " + std::to_string(event.base) + ",\n";
    s += indent + "  " + q("position") + ": " + pointJson(event.position, "") + "\n";
    s += indent + "}";
    return s;
}

std::string replayTimelineJson(const ReplayTimeline& timeline, const std::string& indent) {
    std::string s = indent + "{\n";
    s += indent + "  " + q("schemaName") + ": " + q(timeline.schemaName) + ",\n";
    s += indent + "  " + q("schemaVersion") + ": " + std::to_string(timeline.schemaVersion) + ",\n";
    s += indent + "  " + q("durationSeconds") + ": " + dbl(timeline.durationSeconds) + ",\n";
    s += indent + "  " + q("events") + ": [\n";
    for (std::size_t i = 0; i < timeline.events.size(); ++i) {
        s += replayEventJson(timeline.events[i], indent + "    ");
        if (i + 1 < timeline.events.size()) s += ",";
        s += "\n";
    }
    s += indent + "  ]\n";
    s += indent + "}";
    return s;
}

double safeRate(int num, int den) {
    return den > 0 ? static_cast<double>(num) / den : 0.0;
}

std::string playerJson(const PlayerBoxScore& p, const std::string& indent) {
    const int pa  = p.atBats + p.walks + p.hitByPitch + p.sacFlies;
    const double avg = safeRate(p.hits, p.atBats);
    const double obp = safeRate(p.hits + p.walks + p.hitByPitch, pa > 0 ? pa : 1);
    const double slg = safeRate(p.totalBases, p.atBats);
    // wOBA linear weights
    const int singles = p.hits - p.doubles - p.triples - p.homeRuns;
    const double wobaN = 0.690*p.walks + 0.720*p.hitByPitch + 0.888*singles
                       + 1.271*p.doubles + 1.616*p.triples + 2.101*p.homeRuns;
    const double woba  = pa > 0 ? wobaN / pa : 0.0;

    std::string s = indent + "{\n";
    s += indent + "  " + q("name")       + ": " + q(p.name)                    + ",\n";
    s += indent + "  " + q("atBats")     + ": " + std::to_string(p.atBats)     + ",\n";
    s += indent + "  " + q("hits")       + ": " + std::to_string(p.hits)       + ",\n";
    s += indent + "  " + q("doubles")    + ": " + std::to_string(p.doubles)    + ",\n";
    s += indent + "  " + q("triples")    + ": " + std::to_string(p.triples)    + ",\n";
    s += indent + "  " + q("homeRuns")   + ": " + std::to_string(p.homeRuns)   + ",\n";
    s += indent + "  " + q("walks")      + ": " + std::to_string(p.walks)      + ",\n";
    s += indent + "  " + q("strikeouts") + ": " + std::to_string(p.strikeouts) + ",\n";
    s += indent + "  " + q("totalBases") + ": " + std::to_string(p.totalBases) + ",\n";
    s += indent + "  " + q("rbi")        + ": " + std::to_string(p.rbi)        + ",\n";
    s += indent + "  " + q("avg")        + ": " + dbl(avg)  + ",\n";
    s += indent + "  " + q("obp")        + ": " + dbl(obp)  + ",\n";
    s += indent + "  " + q("slg")        + ": " + dbl(slg)  + ",\n";
    s += indent + "  " + q("ops")        + ": " + dbl(obp + slg) + ",\n";
    s += indent + "  " + q("woba")       + ": " + dbl(woba) + "\n";
    s += indent + "}";
    return s;
}

std::string pitcherJson(const PitcherBoxScore& p, const std::string& indent) {
    const double ip  = p.outsRecorded / 3.0;
    const double era = ip > 0 ? p.earnedRuns * 9.0 / ip : 0.0;
    const double whip = ip > 0 ? (p.walks + p.hitsAllowed) / ip : 0.0;
    const double fip  = ip > 0
        ? (13.0*p.homeRunsAllowed + 3.0*p.walks - 2.0*p.strikeouts) / ip + 3.10 : 0.0;

    std::ostringstream ipStr;
    ipStr << std::fixed << std::setprecision(1) << ip;

    std::string s = indent + "{\n";
    s += indent + "  " + q("name")          + ": " + q(p.name)                       + ",\n";
    s += indent + "  " + q("wins")           + ": " + std::to_string(p.wins)  + ",\n";
    s += indent + "  " + q("saves")          + ": " + std::to_string(p.saves) + ",\n";
    s += indent + "  " + q("ip")            + ": " + ipStr.str()                     + ",\n";
    s += indent + "  " + q("hitsAllowed")   + ": " + std::to_string(p.hitsAllowed)   + ",\n";
    s += indent + "  " + q("earnedRuns")    + ": " + std::to_string(p.earnedRuns)    + ",\n";
    s += indent + "  " + q("walks")         + ": " + std::to_string(p.walks)         + ",\n";
    s += indent + "  " + q("strikeouts")    + ": " + std::to_string(p.strikeouts)    + ",\n";
    s += indent + "  " + q("homeRuns")      + ": " + std::to_string(p.homeRunsAllowed) + ",\n";
    s += indent + "  " + q("era")           + ": " + dbl(era)  + ",\n";
    s += indent + "  " + q("whip")          + ": " + dbl(whip) + ",\n";
    s += indent + "  " + q("fip")           + ": " + dbl(fip)  + "\n";
    s += indent + "}";
    return s;
}

std::string pitcherArrayJson(const std::vector<PitcherBoxScore>& pitchers,
                              const std::string& indent) {
    std::string s = "[\n";
    for (std::size_t i = 0; i < pitchers.size(); ++i) {
        s += pitcherJson(pitchers[i], indent + "  ");
        if (i + 1 < pitchers.size()) s += ",";
        s += "\n";
    }
    return s + indent + "]";
}

std::string playerArrayJson(const std::vector<PlayerBoxScore>& players,
                            const std::string& indent) {
    std::string s = "[\n";
    for (std::size_t i = 0; i < players.size(); ++i) {
        s += playerJson(players[i], indent + "  ");
        if (i + 1 < players.size()) s += ",";
        s += "\n";
    }
    return s + indent + "]";
}

} // namespace

std::string exportGameToJson(const GameEngine& engine) {
    const GameResult r = engine.result();

    std::string s = "{\n";
    s += "  " + q("awayTeam")   + ": " + q(engine.awayTeamName()) + ",\n";
    s += "  " + q("homeTeam")   + ": " + q(engine.homeTeamName()) + ",\n";
    s += "  " + q("awayScore")  + ": " + std::to_string(r.awayScore)  + ",\n";
    s += "  " + q("homeScore")  + ": " + std::to_string(r.homeScore)  + ",\n";
    s += "  " + q("innings")    + ": " + std::to_string(r.innings)    + ",\n";
    s += "  " + q("result")     + ": " + q(resultTypeName(r.type))    + ",\n";

    s += "  " + q("awayLineScore") + ": " + intArray(engine.awayLineScore()) + ",\n";
    s += "  " + q("homeLineScore") + ": " + intArray(engine.homeLineScore()) + ",\n";

    s += "  " + q("awayPlayerStats")  + ": " + playerArrayJson(engine.awayPlayerStats(),   "  ") + ",\n";
    s += "  " + q("homePlayerStats")  + ": " + playerArrayJson(engine.homePlayerStats(),   "  ") + ",\n";
    s += "  " + q("awayPitcherStats") + ": " + pitcherArrayJson(engine.awayPitcherStats(), "  ") + ",\n";
    s += "  " + q("homePitcherStats") + ": " + pitcherArrayJson(engine.homePitcherStats(), "  ") + "\n";

    s += "}\n";
    return s;
}

std::string exportAnimationPlanToJson(const AnimationPlan& plan) {
    std::string s = "{\n";
    s += "  " + q("gameId") + ": " + q(plan.gameId) + ",\n";
    s += "  " + q("description") + ": " + q(plan.description) + ",\n";
    s += "  " + q("totalDurationSeconds") + ": " + dbl(plan.totalDurationSeconds) + ",\n";
    s += "  " + q("hasPitch") + ": " + std::string(plan.hasPitch ? "true" : "false") + ",\n";
    s += "  " + q("hasBattedBall") + ": " + std::string(plan.hasBattedBall ? "true" : "false") + ",\n";
    s += "  " + q("pitch") + ": " + pitchAnimationJson(plan.pitch, "  ") + ",\n";
    s += "  " + q("battedBall") + ": " + battedBallAnimationJson(plan.battedBall, "  ") + ",\n";
    s += "  " + q("runners") + ": " + objectArrayJson(
        plan.runners, "  ", runnerAnimationJson) + ",\n";
    s += "  " + q("defenders") + ": " + objectArrayJson(
        plan.defenders, "  ", defenseAnimationJson) + ",\n";
    s += "  " + q("throws") + ": " + objectArrayJson(
        plan.throws, "  ", throwAnimationJson) + ",\n";
    s += "  " + q("runnerMovements") + ": " + objectArrayJson(
        plan.runnerMovements, "  ", runnerMovementJson) + ",\n";
    s += "  " + q("throwMovements") + ": " + objectArrayJson(
        plan.throwMovements, "  ", throwMovementJson) + ",\n";
    s += "  " + q("tagPlays") + ": " + objectArrayJson(
        plan.tagPlays, "  ", tagPlayJson) + ",\n";
    s += "  " + q("defensiveDecision") + ": "
        + replayDecisionJson(plan.defensiveDecision, "  ") + ",\n";
    s += "  " + q("replayTimeline") + ": " + replayTimelineJson(plan.replayTimeline, "  ") + "\n";
    s += "}\n";
    return s;
}

std::string exportSeasonSummaryToJson(const std::vector<TeamSeasonSummary>& teams,
                                      const std::string& worldSeriesChampion,
                                      int seasonNumber) {
    std::string s = "{\n";
    s += "  " + q("season")              + ": " + std::to_string(seasonNumber) + ",\n";
    s += "  " + q("worldSeriesChampion") + ": " + q(worldSeriesChampion)       + ",\n";
    s += "  " + q("teams") + ": [\n";
    for (std::size_t i = 0; i < teams.size(); ++i) {
        const auto& t = teams[i];
        std::string ts = "    {\n";
        ts += "      " + q("name")    + ": " + q(t.name)    + ",\n";
        ts += "      " + q("league")  + ": " + q(t.league)  + ",\n";
        ts += "      " + q("wins")    + ": " + std::to_string(t.wins)   + ",\n";
        ts += "      " + q("losses")  + ": " + std::to_string(t.losses) + ",\n";
        ts += "      " + q("rsPerG")  + ": " + dbl(t.rsPerG, 2)  + ",\n";
        ts += "      " + q("raPerG")  + ": " + dbl(t.raPerG, 2)  + ",\n";
        ts += "      " + q("teamEra") + ": " + dbl(t.teamEra, 2) + ",\n";
        ts += "      " + q("ba")      + ": " + dbl(t.teamBa, 3)  + ",\n";
        ts += "      " + q("obp")     + ": " + dbl(t.teamObp, 3) + ",\n";
        ts += "      " + q("slg")     + ": " + dbl(t.teamSlg, 3) + ",\n";
        ts += "      " + q("woba")    + ": " + dbl(t.teamWoba, 3)+ ",\n";
        ts += "      " + q("kPct")    + ": " + dbl(t.kPct, 1)    + ",\n";
        ts += "      " + q("bbPct")   + ": " + dbl(t.bbPct, 1)   + "\n";
        ts += "    }";
        s += ts;
        if (i + 1 < teams.size()) s += ",";
        s += "\n";
    }
    s += "  ]\n}\n";
    return s;
}

} // namespace joji
