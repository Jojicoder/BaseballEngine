#include "AtBatEngine.h"

#include <algorithm>
#include <utility>

namespace joji {

namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

} // namespace

AtBatEngine::AtBatEngine(BallPhysicsEngine physicsEngine,
                         PitchEngine pitchEngine,
                         SwingDecisionEngine swingDecisionEngine,
                         ZoneJudge zoneJudge,
                         SwingEngine swingEngine,
                         ContactEngine contactEngine)
    : physicsEngine_(std::move(physicsEngine)),
      pitchEngine_(std::move(pitchEngine)),
      swingDecisionEngine_(std::move(swingDecisionEngine)),
      zoneJudge_(std::move(zoneJudge)),
      swingEngine_(std::move(swingEngine)),
      contactEngine_(std::move(contactEngine)) {}

PlayResult AtBatEngine::simulate(const Player& batter, const Player& pitcher, Random& random) const {
    const double contactScore = (batter.contact - 50) * 0.0035;
    const double powerScore = (batter.power - 50) * 0.0025;
    const double eyeScore = (batter.eye - 50) * 0.0030;
    const double pitcherControl = (pitcher.pitchingControl - 50) * 0.0030;
    const double pitcherStuff = (pitcher.pitchingStuff - 50) * 0.0035;
    const double pitcherVelocity = (pitcher.pitchingVelocity - 50) * 0.0020;

    const double walkProbability = clamp(0.085 + eyeScore - pitcherControl, 0.035, 0.16);
    const double hitByPitchProbability = clamp(0.010 - pitcherControl * 0.5, 0.004, 0.020);
    const double strikeOutProbability = clamp(0.205 - contactScore + pitcherStuff + pitcherVelocity, 0.08, 0.34);
    const double homeRunProbability = clamp(0.032 + powerScore - pitcherStuff * 0.45, 0.008, 0.085);
    const double tripleProbability = clamp(0.008 + (batter.speed - 50) * 0.00018, 0.002, 0.025);
    const double doubleProbability = clamp(0.052 + powerScore * 0.8 + (batter.speed - 50) * 0.00008, 0.025, 0.11);
    const double singleProbability = clamp(0.155 + contactScore - pitcherStuff * 0.55, 0.08, 0.25);

    const double roll = random.real(0.0, 1.0);
    double threshold = walkProbability;

    PlayResult result;
    result.batterName = batter.name;
    result.pitcherName = pitcher.name;

    if (roll < threshold) {
        result.type = AtBatResultType::Walk;
        return result;
    }

    threshold += hitByPitchProbability;
    if (roll < threshold) {
        result.type = AtBatResultType::HitByPitch;
        return result;
    }

    threshold += strikeOutProbability;
    if (roll < threshold) {
        result.type = AtBatResultType::StrikeOut;
        return result;
    }

    threshold += homeRunProbability;
    if (roll < threshold) {
        result.type = AtBatResultType::HomeRun;
    } else {
        threshold += tripleProbability;
        if (roll < threshold) {
            result.type = AtBatResultType::Triple;
        } else {
            threshold += doubleProbability;
            if (roll < threshold) {
                result.type = AtBatResultType::Double;
            } else {
                threshold += singleProbability;
                if (roll < threshold) {
                    result.type = AtBatResultType::Single;
                } else {
                    result.type = random.chance(0.56) ? AtBatResultType::GroundOut : AtBatResultType::FlyOut;
                }
            }
        }
    }

    result.battedBall = physicsEngine_.generateFromContact(batter.power, pitcher.pitchingStuff, result.type, random);
    return result;
}

AtBatResult AtBatEngine::simulatePlateAppearance(const Player& batter,
                                                 const Player& pitcher,
                                                 Random& random,
                                                 bool buntIntent) const {
    AtBatState state = startAtBat(batter, pitcher);
    state.buntIntent = buntIntent;
    while (!simulateNextPitch(state, random)) {}

    AtBatResult result;
    result.batter = state.batter;
    result.pitcher = state.pitcher;
    result.finalOutcome = state.finalOutcome.value_or(AtBatOutcome::StrikeOut);
    result.finalCount = state.count;
    result.pitchLogs = state.pitchLogs;
    result.baseRunningEvents = state.baseRunningEvents;
    result.battedBallInput = state.battedBallInput;
    return result;
}

AtBatState AtBatEngine::startAtBat(const Player& batter, const Player& pitcher) const {
    AtBatState state;
    state.batter = batter;
    state.pitcher = pitcher;
    return state;
}

bool AtBatEngine::simulateNextPitch(AtBatState& state, Random& random) const {
    if (state.isFinished) return true;

    // ── Bunt override ─────────────────────────────────────────────────────
    if (state.buntIntent) {
        // Generate pitch first so we have location info
        PitchLog log;
        log.pitchNumber = state.pitchNumber;
        log.countBefore = state.count;
        log.pitch = pitchEngine_.generate(state.pitcher, state.batter, state.count, state.lastPitch, random);

        const double contact = clamp((state.batter.contact - 50) / 50.0, -0.8, 0.8);
        const double pitchQuality = clamp(log.pitch.pitchQuality, 0.0, 1.0);
        const bool buntablePitch = std::abs(log.pitch.locationX) <= 1.15
                                && log.pitch.locationZ >= 1.15
                                && log.pitch.locationZ <= 3.85;
        const double takeBadPitch = buntablePitch ? 0.0 : 0.34;

        // Decide whether to offer. Bad pitches are often pulled back.
        const bool attemptBunt = random.chance(std::clamp(0.78 - takeBadPitch + contact * 0.07,
                                                          0.35,
                                                          0.86));

        if (attemptBunt) {
            const double roll = random.real(0.0, 1.0);
            const double goodBuntProb = std::clamp(0.48 + contact * 0.10
                                                       - pitchQuality * 0.12
                                                       + (buntablePitch ? 0.08 : -0.16),
                                                   0.28,
                                                   0.66);
            const double foulBuntProb = std::clamp(0.27 - contact * 0.035
                                                       + pitchQuality * 0.06
                                                       + (buntablePitch ? 0.01 : 0.10),
                                                   0.16,
                                                   0.39);

            if (roll < goodBuntProb) {
                // Good bunt — InPlay
                BattedBallInput buntInput;
                buntInput.exitVelocity = random.real(24.0, 52.0) - contact * 3.0;
                buntInput.launchAngle  = random.real(-10.0, 5.0);
                buntInput.sprayAngle   = random.real(-32.0, 32.0);
                buntInput.isBunt       = true;

                log.swingDecision.decision = SwingDecisionType::Swing;
                log.contactResult = ContactResult{};
                log.contactResult->resultType = ContactResultType::InPlay;
                log.contactResult->battedBallInput = buntInput;
                log.pitchOutcome = PitchOutcome::InPlay;
                log.countAfter = state.count;

                state.count = log.countAfter;
                state.pitchLogs.push_back(log);
                ++state.pitchNumber;

                state.finalOutcome = AtBatOutcome::InPlay;
                state.battedBallInput = buntInput;
                state.isFinished = true;
                return true;

            } else if (roll < goodBuntProb + foulBuntProb) {
                // Foul bunt
                log.swingDecision.decision = SwingDecisionType::Swing;
                log.contactResult = ContactResult{};
                log.contactResult->resultType = ContactResultType::Foul;

                if (log.countBefore.strikes == 2) {
                    // Foul bunt on 2 strikes = strikeout
                    log.pitchOutcome = PitchOutcome::SwingingStrike;
                    log.countAfter = Count{log.countBefore.balls, 3};
                    state.count = log.countAfter;
                    state.pitchLogs.push_back(log);
                    ++state.pitchNumber;
                    state.finalOutcome = AtBatOutcome::StrikeOut;
                    state.isFinished = true;
                    return true;
                } else {
                    log.pitchOutcome = PitchOutcome::Foul;
                    log.countAfter = afterFoul(log.countBefore);
                    state.count = log.countAfter;
                    state.pitchLogs.push_back(log);
                    ++state.pitchNumber;
                    return false; // AB continues
                }

            } else {
                // Missed bunt — swinging strike
                log.swingDecision.decision = SwingDecisionType::Swing;
                log.contactResult = ContactResult{};
                log.contactResult->resultType = ContactResultType::SwingMiss;
                log.pitchOutcome = PitchOutcome::SwingingStrike;
                log.countAfter = afterStrike(log.countBefore);
                state.count = log.countAfter;
                state.pitchLogs.push_back(log);
                ++state.pitchNumber;

                if (isStrikeOut(state.count)) {
                    state.finalOutcome = AtBatOutcome::StrikeOut;
                    state.isFinished = true;
                    return true;
                }
                return false; // AB continues
            }
        }
        // !attemptBunt: batter shows bunt but takes the pitch — fall through to normal logic
        // (don't push log yet; let simulatePitch handle it from scratch)
    }
    // ──────────────────────────────────────────────────────────────────────

    PitchLog log = simulatePitch(state.batter, state.pitcher, state.count, state.pitchNumber, random);
    state.count = log.countAfter;
    state.lastPitch = log.pitch;
    state.pitchLogs.push_back(log);
    ++state.pitchNumber;

    if (log.pitchOutcome == PitchOutcome::InPlay) {
        state.finalOutcome = AtBatOutcome::InPlay;
        if (log.contactResult.has_value()) {
            state.battedBallInput = log.contactResult->battedBallInput;
        }
        state.isFinished = true;
        return true;
    }
    if (isWalk(state.count)) {
        state.finalOutcome = AtBatOutcome::Walk;
        state.isFinished = true;
        return true;
    }
    if (isStrikeOut(state.count)) {
        state.finalOutcome = AtBatOutcome::StrikeOut;
        state.isFinished = true;
        return true;
    }
    return false;
}

PitchLog AtBatEngine::simulatePitch(const Player& batter,
                                    const Player& pitcher,
                                    const Count& count,
                                    int pitchNumber,
                                    Random& random) const {
    PitchLog log;
    log.pitchNumber = pitchNumber;
    log.countBefore = count;
    log.pitch = pitchEngine_.generate(pitcher, batter, count, std::nullopt, random);
    log.swingDecision = swingDecisionEngine_.decide(batter, log.pitch, count, pitcher.throwingHand, random);

    if (log.swingDecision.decision == SwingDecisionType::Take) {
        log.zoneResult = zoneJudge_.judge(log.pitch);
        if (log.zoneResult->result == ZoneResultType::Ball) {
            log.pitchOutcome = PitchOutcome::Ball;
            log.countAfter = afterBall(count);
        } else {
            log.pitchOutcome = PitchOutcome::CalledStrike;
            log.countAfter = afterStrike(count);
        }
        return log;
    }

    log.swing = swingEngine_.generate(batter, log.pitch, count, log.swingDecision, random);
    log.contactResult = contactEngine_.resolve(batter, pitcher, log.pitch, *log.swing, count, random);

    switch (log.contactResult->resultType) {
        case ContactResultType::SwingMiss:
            log.pitchOutcome = PitchOutcome::SwingingStrike;
            log.countAfter = afterStrike(count);
            break;
        case ContactResultType::Foul:
            if (log.contactResult->battedBallInput.has_value()) {
                log.pitchOutcome = PitchOutcome::InPlay;
                log.countAfter = count;
            } else {
                log.pitchOutcome = PitchOutcome::Foul;
                log.countAfter = afterFoul(count);
            }
            break;
        case ContactResultType::InPlay:
            log.pitchOutcome = PitchOutcome::InPlay;
            log.countAfter = count;
            break;
    }

    return log;
}

std::string toString(AtBatResultType type) {
    switch (type) {
        case AtBatResultType::StrikeOut:
            return "strikes out";
        case AtBatResultType::Walk:
            return "walks";
        case AtBatResultType::GroundOut:
            return "grounds out";
        case AtBatResultType::FlyOut:
            return "flies out";
        case AtBatResultType::Single:
            return "hits a single";
        case AtBatResultType::Double:
            return "hits a double";
        case AtBatResultType::Triple:
            return "hits a triple";
        case AtBatResultType::HomeRun:
            return "hits a home run";
        case AtBatResultType::Error:
            return "reaches on error";
        case AtBatResultType::FielderChoice:
            return "reaches on fielder's choice";
        case AtBatResultType::InfieldFly:
            return "hits an infield fly (automatic out)";
        case AtBatResultType::SacrificeBunt:
            return "lays down a sacrifice bunt";
        case AtBatResultType::SqueezeBunt:
            return "executes the squeeze";
        case AtBatResultType::BuntSingle:
            return "beats out a bunt single";
        case AtBatResultType::BuntFielderChoice:
            return "reaches on bunt fielder's choice";
        case AtBatResultType::HitByPitch:
            return "is hit by a pitch";
    }
    return "finishes the at-bat";
}

} // namespace joji
