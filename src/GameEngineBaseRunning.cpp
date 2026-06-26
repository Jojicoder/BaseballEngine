#include "GameEngine.h"
#include "RunExpectancy.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace joji {

void GameEngine::advanceRunnersForWalk(PlayResult& result) {
    if (state_.bases[0] && state_.bases[1] && state_.bases[2]) {
        scoreRunner(result, *state_.bases[2]);
        state_.bases[2] = state_.bases[1];
        state_.bases[1] = state_.bases[0];
    } else if (state_.bases[0] && state_.bases[1]) {
        state_.bases[2] = state_.bases[1];
        state_.bases[1] = state_.bases[0];
    } else if (state_.bases[0]) {
        state_.bases[1] = state_.bases[0];
    }

    state_.bases[0] = result.batterName;
    result.events.push_back(result.batterName + " takes first base.");
}

void GameEngine::advanceAllRunnersOneBase(const std::string& reason) {
    if (state_.bases[2].has_value()) {
        const std::string runner = *state_.bases[2];
        state_.bases[2].reset();
        if (state_.isTop) { state_.awayScore += 1; }
        else              { state_.homeScore += 1; }
        currentHalfInningRuns_ += 1;
        currentPitcherRunsAllowed() += 1;
        logs_.push_back(GameLog{state_.inning, state_.isTop, runner + " scores on " + reason + "."});
    }
    state_.bases[2] = state_.bases[1];
    state_.bases[1] = state_.bases[0];
    state_.bases[0].reset();
}

void GameEngine::checkPickoff() {
    if (!atBatInProgress_) return;

    for (int baseIdx = 0; baseIdx <= 1; ++baseIdx) {
        if (!state_.bases[static_cast<std::size_t>(baseIdx)].has_value()) continue;
        const std::string runner = *state_.bases[static_cast<std::size_t>(baseIdx)];
        const int baseNum = baseIdx + 1;

        const double attemptProb = (baseIdx == 0) ? 0.012 : 0.003;
        if (!random_.chance(attemptProb)) continue;
        battingBoxScore().pickoffAttempts++;

        const int afterPitch = static_cast<int>(currentAtBat_.pitchLogs.size());

        int runnerSpd = 55;
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == runner) { runnerSpd = p.speed; break; }
        }
        const FieldPosition targetPos = (baseIdx == 0) ? FieldPosition::FirstBase
                                                       : FieldPosition::SecondBase;
        double fielderArm = 128.0;
        for (const auto& f : pitchingDefenseAlignment().fielders) {
            if (f.position == targetPos) { fielderArm = f.armStrength; break; }
        }

        const double spdFactor  = (runnerSpd - 55) * 0.005;
        const double armFactor  = (fielderArm - 128.0) * 0.002;
        const double outProb    = std::clamp(0.30 - spdFactor + armFactor, 0.12, 0.52);

        if (random_.chance(outProb)) {
            battingBoxScore().pickoffOuts++;
            state_.bases[static_cast<std::size_t>(baseIdx)].reset();
            state_.outs += 1;
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "PO: " + runner + " picked off at " + std::to_string(baseNum) + "B!"});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::PickoffOut, runner, baseNum, baseNum, false, afterPitch});
        } else {
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "PO attempt: " + runner + " safe at " + std::to_string(baseNum) + "B."});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::PickoffAttempt, runner, baseNum, baseNum, false, afterPitch});
        }
        break;
    }
}

void GameEngine::checkStolenBase() {
    if (!atBatInProgress_) return;
    const Count& count = currentAtBat_.count;
    if (count.balls == 3 && count.strikes == 0) return;
    if (state_.outs >= 3) return;

    double tempoAdj = 0.0;
    if (!currentAtBat_.pitchLogs.empty()) {
        const auto& lastLog = currentAtBat_.pitchLogs.back();
        const double vel = lastLog.pitch.pitchVelocity;
        tempoAdj = -(vel - 88.0) * 0.001;
        const PitchType pt = lastLog.pitch.pitchType;
        if (pt == PitchType::Curveball || pt == PitchType::Changeup || pt == PitchType::Splitter) {
            tempoAdj += 0.010;
        }
    }

    double catcherArm = 128.0;
    for (const auto& f : pitchingDefenseAlignment().fielders) {
        if (f.position == FieldPosition::Catcher) { catcherArm = f.armStrength; break; }
    }
    const double armFactor = (catcherArm - 128.0) * 0.003;

    auto attemptSteal = [&](const std::string& runner, int fromBase, int toBase,
                             double baseAttemptProb, double baseSuccessProb) {
        int runnerSpd = 55;
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == runner) { runnerSpd = p.speed; break; }
        }
        const double spdFactor = (runnerSpd - 55) * 0.004;
        const double successProb = std::clamp(baseSuccessProb + spdFactor - armFactor, 0.38, 0.92);
        const RunExpectancyDelta re = stealRunExpectancyDelta(state_, fromBase, toBase);
        const double edge = successProb - re.breakEvenSuccessRate;
        const double reAggression = edge < -0.04 ? 0.22
                                  : edge <  0.02 ? 0.55
                                  : edge >  0.14 ? 1.20
                                  : 1.0;
        const double prob = std::clamp((baseAttemptProb + spdFactor + tempoAdj) * reAggression,
                                       0.001,
                                       0.16);
        if (!random_.chance(prob)) return;

        const std::string baseName = (toBase == 2) ? "2B" : "3B";
        battingBoxScore().stolenBaseAttempts++;
        const bool success = random_.chance(successProb);
        const int afterPitch = static_cast<int>(currentAtBat_.pitchLogs.size());

        if (success) {
            battingBoxScore().stolenBases++;
            state_.bases[static_cast<std::size_t>(fromBase - 1)].reset();
            state_.bases[static_cast<std::size_t>(toBase - 1)] = runner;
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "SB: " + runner + " steals " + baseName + "."});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::StolenBase, runner, fromBase, toBase, false, afterPitch});
            for (auto& p : battingPlayerStats()) {
                if (p.name == runner) { p.stolenBases++; break; }
            }
        } else {
            battingBoxScore().caughtStealing++;
            state_.bases[static_cast<std::size_t>(fromBase - 1)].reset();
            state_.outs += 1;
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "CS: " + runner + " caught stealing " + baseName + "."});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::CaughtStealing, runner, fromBase, toBase, false, afterPitch});
            for (auto& p : battingPlayerStats()) {
                if (p.name == runner) { p.caughtStealing++; break; }
            }
            for (auto& p : pitchingPlayerStats()) {
                if (p.position == Position::Catcher) { p.caughtStealing++; break; }
            }
        }
    };

    const bool on1B = state_.bases[0].has_value();
    const bool on2B = state_.bases[1].has_value();
    const bool on3B = state_.bases[2].has_value();

    const int battingScoreSB = state_.isTop ? state_.awayScore : state_.homeScore;
    const int pitchingScoreSB = state_.isTop ? state_.homeScore : state_.awayScore;
    const int leadSB = battingScoreSB - pitchingScoreSB;
    if (leadSB >= 4 || leadSB <= -4) return;

    double situMult = 1.0;
    if (std::abs(leadSB) <= 1 && state_.inning >= 7) situMult = 1.25;
    else if (std::abs(leadSB) <= 1)                  situMult = 0.90;
    else if (leadSB >= 2)                             situMult = 0.45;
    if (state_.outs == 2) situMult *= 1.12;

    if (on1B && on2B && !on3B) {
        const std::string r1 = *state_.bases[0];
        const std::string r2 = *state_.bases[1];
        int spd1 = 55;
        for (const auto& p : battingTeam().lineup())
            if (p.name == r1) { spd1 = p.speed; break; }
        const double spdFactor1 = (spd1 - 55) * 0.004;
        if (random_.chance(std::clamp(0.032 * situMult + spdFactor1 + tempoAdj, 0.001, 0.11))) {
            const int afterPitch = static_cast<int>(currentAtBat_.pitchLogs.size());
            battingBoxScore().stolenBaseAttempts += 2;
            int spd2 = 55;
            for (const auto& p : battingTeam().lineup())
                if (p.name == r2) { spd2 = p.speed; break; }
            const double spdF2 = (spd2 - 55) * 0.004;
            const bool leadSuccess = random_.chance(std::clamp(0.77 + spdF2 - armFactor, 0.38, 0.92));
            const bool trailSuccess = random_.chance(std::clamp(0.81 + spdFactor1 - armFactor * 0.6, 0.42, 0.92));

            if (leadSuccess) {
                battingBoxScore().stolenBases++;
                state_.bases[1].reset();
                state_.bases[2] = r2;
                logs_.push_back(GameLog{state_.inning, state_.isTop, "SB: " + r2 + " steals 3B (double steal)."});
                currentAtBat_.baseRunningEvents.push_back(
                    {BaseRunningEventType::StolenBase, r2, 2, 3, false, afterPitch});
                for (auto& p : battingPlayerStats()) if (p.name == r2) { p.stolenBases++; break; }
            } else {
                battingBoxScore().caughtStealing++;
                state_.bases[1].reset();
                state_.outs += 1;
                logs_.push_back(GameLog{state_.inning, state_.isTop, "CS: " + r2 + " caught stealing 3B."});
                currentAtBat_.baseRunningEvents.push_back(
                    {BaseRunningEventType::CaughtStealing, r2, 2, 3, false, afterPitch});
                for (auto& p : battingPlayerStats()) if (p.name == r2) { p.caughtStealing++; break; }
                for (auto& p : pitchingPlayerStats()) if (p.position == Position::Catcher) { p.caughtStealing++; break; }
            }
            if (state_.outs < 3) {
                if (trailSuccess) {
                    battingBoxScore().stolenBases++;
                    state_.bases[0].reset();
                    state_.bases[1] = r1;
                    logs_.push_back(GameLog{state_.inning, state_.isTop, "SB: " + r1 + " steals 2B (double steal)."});
                    currentAtBat_.baseRunningEvents.push_back(
                        {BaseRunningEventType::StolenBase, r1, 1, 2, false, afterPitch});
                    for (auto& p : battingPlayerStats()) if (p.name == r1) { p.stolenBases++; break; }
                } else {
                    battingBoxScore().caughtStealing++;
                    state_.bases[0].reset();
                    state_.outs += 1;
                    logs_.push_back(GameLog{state_.inning, state_.isTop, "CS: " + r1 + " caught stealing 2B."});
                    currentAtBat_.baseRunningEvents.push_back(
                        {BaseRunningEventType::CaughtStealing, r1, 1, 2, false, afterPitch});
                    for (auto& p : battingPlayerStats()) if (p.name == r1) { p.caughtStealing++; break; }
                }
            }
        }
        return;
    }

    if (on1B && !on2B) {
        attemptSteal(*state_.bases[0], 1, 2, 0.055 * situMult, 0.83);
    }

    if (on2B && !on3B && !on1B && state_.outs < 3) {
        attemptSteal(*state_.bases[1], 2, 3, 0.025 * situMult, 0.77);
    }
}

void GameEngine::checkWildPitchPassedBall() {
    if (!atBatInProgress_ || currentAtBat_.pitchLogs.empty()) return;
    const auto& log = currentAtBat_.pitchLogs.back();
    if (log.pitchOutcome == PitchOutcome::InPlay) return;
    if (!state_.bases[0].has_value() && !state_.bases[1].has_value() && !state_.bases[2].has_value()) return;

    const double vel    = log.pitch.pitchVelocity;
    const double wpProb = std::clamp(0.014 + (vel - 85.0) * 0.00022, 0.006, 0.030);
    const double pbProb = 0.005;
    const int    after  = static_cast<int>(currentAtBat_.pitchLogs.size());

    BaseRunningEventType evType;
    if (random_.chance(wpProb)) {
        evType = BaseRunningEventType::WildPitch;
        pitchingBoxScore().wildPitches++;
        logs_.push_back(GameLog{state_.inning, state_.isTop, "Wild pitch!"});
    } else if (random_.chance(pbProb)) {
        evType = BaseRunningEventType::PassedBall;
        pitchingBoxScore().passedBalls++;
        logs_.push_back(GameLog{state_.inning, state_.isTop, "Passed ball!"});
    } else {
        return;
    }

    if (state_.bases[2].has_value()) {
        currentAtBat_.baseRunningEvents.push_back(
            {evType, *state_.bases[2], 3, 4, true, after});
    }
    if (state_.bases[1].has_value()) {
        currentAtBat_.baseRunningEvents.push_back(
            {evType, *state_.bases[1], 2, 3, false, after});
    }
    if (state_.bases[0].has_value()) {
        currentAtBat_.baseRunningEvents.push_back(
            {evType, *state_.bases[0], 1, 2, false, after});
    }
    advanceAllRunnersOneBase(evType == BaseRunningEventType::WildPitch ? "wild pitch" : "passed ball");
}

void GameEngine::checkBalk() {
    if (!atBatInProgress_) return;
    const bool runnersOn = state_.bases[0].has_value()
                        || state_.bases[1].has_value()
                        || state_.bases[2].has_value();
    if (!runnersOn) return;

    if (random_.chance(0.0015)) {
        pitchingBoxScore().balks++;
        logs_.push_back(GameLog{state_.inning, state_.isTop, "Balk! All runners advance."});
        const int after = static_cast<int>(currentAtBat_.pitchLogs.size());
        if (state_.bases[2].has_value())
            currentAtBat_.baseRunningEvents.push_back(
                {BaseRunningEventType::Balk, *state_.bases[2], 3, 4, true, after});
        if (state_.bases[1].has_value())
            currentAtBat_.baseRunningEvents.push_back(
                {BaseRunningEventType::Balk, *state_.bases[1], 2, 3, false, after});
        if (state_.bases[0].has_value())
            currentAtBat_.baseRunningEvents.push_back(
                {BaseRunningEventType::Balk, *state_.bases[0], 1, 2, false, after});
        advanceAllRunnersOneBase("balk");
    }
}

void GameEngine::scoreRunner(PlayResult& result, const std::string& runnerName) {
    if (state_.isTop) {
        state_.awayScore += 1;
    } else {
        state_.homeScore += 1;
    }
    result.runsScored += 1;
    currentHalfInningRuns_ += 1;
    currentPitcherRunsAllowed() += 1;
    result.events.push_back(runnerName + " scores.");

    for (auto& p : battingPlayerStats()) {
        if (p.name == runnerName) { p.runs++; break; }
    }

    PitcherSitTracker& sit = currentSit();
    auto it = std::find(sit.inheritedRunners.begin(), sit.inheritedRunners.end(), runnerName);
    if (it != sit.inheritedRunners.end()) {
        sit.inheritedRunnersScored++;
        sit.inheritedRunners.erase(it);
    }

    if (sit.inSaveSit && !sit.blownSave) {
        const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
        const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
        if (oppScore >= myScore) sit.blownSave = true;
    }
}

} // namespace joji
