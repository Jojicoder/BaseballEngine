#include "AnimationPlanBuilder.h"
#include "GameEngine.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace joji {

namespace {

double clampValue(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

// 走者刺殺: 送球した野手にアシスト+外野補殺、本塁カバーの捕手にプットアウト
void recordThrowOutAtHome(std::vector<PlayerBoxScore>& defenders,
                          const std::string& fielderName) {
    for (auto& p : defenders) {
        if (p.name == fielderName) { p.assists++; p.outfieldAssists++; break; }
    }
    for (auto& p : defenders) {
        if (p.position == Position::Catcher) { p.putouts++; break; }
    }
}

FieldPosition toFieldPosition(Position pos) {
    switch (pos) {
        case Position::Pitcher:     return FieldPosition::Pitcher;
        case Position::Catcher:     return FieldPosition::Catcher;
        case Position::FirstBase:   return FieldPosition::FirstBase;
        case Position::SecondBase:  return FieldPosition::SecondBase;
        case Position::ThirdBase:   return FieldPosition::ThirdBase;
        case Position::Shortstop:   return FieldPosition::Shortstop;
        case Position::LeftField:   return FieldPosition::LeftField;
        case Position::CenterField: return FieldPosition::CenterField;
        case Position::RightField:  return FieldPosition::RightField;
    }
    return FieldPosition::CenterField;
}

// Fallback: 守備位置 → ラインアップインデックス (position 未設定チーム向け)
std::size_t lineupIndexForPosition(FieldPosition position) {
    switch (position) {
        case FieldPosition::Pitcher:     return 8;
        case FieldPosition::Catcher:     return 7;
        case FieldPosition::FirstBase:   return 2;
        case FieldPosition::SecondBase:  return 5;
        case FieldPosition::ThirdBase:   return 4;
        case FieldPosition::Shortstop:   return 3;
        case FieldPosition::LeftField:   return 1;
        case FieldPosition::CenterField: return 0;
        case FieldPosition::RightField:  return 6;
    }
    return 0;
}

void applyPlayerStats(Fielder& fielder, const Player& player) {
    const double speedFactor    = clampValue(0.88 + static_cast<double>(player.speed)    / 420.0, 0.88, 1.10);
    const double fieldingFactor = clampValue((static_cast<double>(player.fielding) - 50.0) / 50.0, -0.8, 0.8);
    const double armFactor      = clampValue(0.86 + static_cast<double>(player.arm)      / 360.0, 0.86, 1.12);
    fielder.name             = player.name;
    fielder.speedFeetPerSecond *= speedFactor;
    fielder.reactionSeconds  = clampValue(fielder.reactionSeconds - fieldingFactor * 0.035, 0.18, 0.65);
    fielder.fielding         = clampValue(fielder.fielding + fieldingFactor * 0.08, 0.62, 0.98);
    fielder.armStrength      *= armFactor;
    fielder.routeEfficiency  = clampValue(fielder.routeEfficiency + fieldingFactor * 0.035, 0.78, 0.99);
}

// fieldingForm を適用した Player コピーを返す
Player applyFieldingForm(const Player& p, const std::unordered_map<std::string, PlayerGameForm>& forms) {
    auto it = forms.find(p.name);
    if (it == forms.end()) return p;
    Player formed = p;
    const double f = it->second.fieldingForm;
    formed.fielding = std::max(0, static_cast<int>(formed.fielding * f));
    formed.arm      = std::max(0, static_cast<int>(formed.arm      * f));
    formed.speed    = std::max(0, static_cast<int>(formed.speed    * f));
    return formed;
}

DefenseAlignment buildDefenseAlignment(const Team& team, const Player& pitcher,
    const std::unordered_map<std::string, PlayerGameForm>& forms) {
    DefenseAlignment defense = DefenseAlignment::standard();
    const auto& lineup = team.lineup();
    if (lineup.empty()) return defense;

    for (auto& fielder : defense.fielders) {
        if (fielder.position == FieldPosition::Pitcher) {
            applyPlayerStats(fielder, applyFieldingForm(pitcher, forms));
            continue;
        }
        const Player* matched = nullptr;
        for (const auto& p : lineup) {
            if (toFieldPosition(p.position) == fielder.position) {
                matched = &p;
                break;
            }
        }
        if (matched) {
            applyPlayerStats(fielder, applyFieldingForm(*matched, forms));
        } else {
            const std::size_t idx = lineupIndexForPosition(fielder.position);
            applyPlayerStats(fielder, applyFieldingForm(
                lineup.at(std::min(idx, lineup.size() - 1)), forms));
        }
    }

    return defense;
}

AtBatResult toAtBatResult(const AtBatState& state) {
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

AnimationPlan pitchOnlyAnimationPlan(const AtBatResult& atBat) {
    AnimationPlanBuilder builder;
    AnimationPlan plan;
    plan.description = atBat.batter.name + " vs " + atBat.pitcher.name;
    plan.pitch = builder.buildPitchAnimation(atBat);
    plan.hasPitch = !plan.pitch.points.empty();
    plan.totalDurationSeconds = plan.pitch.durationSeconds;
    return plan;
}

AnimationPlan pitchOnlyAnimationPlan(const AtBatState& state) {
    return pitchOnlyAnimationPlan(toAtBatResult(state));
}

AnimationPoint runnerBasePoint(int base, double timeSeconds) {
    switch (base) {
        case 1: return {90.0, 90.0, 0.0, timeSeconds};
        case 2: return {0.0, 180.0, 0.0, timeSeconds};
        case 3: return {-90.0, 90.0, 0.0, timeSeconds};
        case 4: return {0.0, 0.0, 0.0, timeSeconds};
        case 0:
        default:
            return {0.0, 0.0, 0.0, timeSeconds};
    }
}

bool supportsRunnerAnimation(AtBatResultType type) {
    return type == AtBatResultType::Walk
        || type == AtBatResultType::HitByPitch
        || type == AtBatResultType::Single
        || type == AtBatResultType::Double
        || type == AtBatResultType::Triple
        || type == AtBatResultType::HomeRun
        || type == AtBatResultType::FielderChoice
        || type == AtBatResultType::GroundOut
        || type == AtBatResultType::FlyOut
        || type == AtBatResultType::SacrificeBunt
        || type == AtBatResultType::SqueezeBunt
        || type == AtBatResultType::BuntSingle
        || type == AtBatResultType::BuntFielderChoice;
}

bool runnerScoredOnPlay(const PlayResult& result, const std::string& runnerName) {
    const std::string scoredEvent = runnerName + " scores.";
    return std::find(result.events.begin(), result.events.end(), scoredEvent) != result.events.end();
}

std::vector<AnimationPoint> makeRunnerPath(int fromBase, int toBase, double secondsPerBase) {
    std::vector<AnimationPoint> points;
    const int finalBase = std::clamp(toBase, 0, 4);
    const int startBase = std::clamp(fromBase, 0, 3);
    if (finalBase <= startBase) {
        points.push_back(runnerBasePoint(startBase, 0.0));
        return points;
    }

    const int baseCount = finalBase - startBase;
    double duration = secondsPerBase * static_cast<double>(baseCount);
    if (secondsPerBase > 0.0) {
        constexpr double RunnerVisualSpeedScale = 1.45;
        // Realistic visual timing: batter-runner to 1B ~3.8s, advancing runner ~3.5s/base.
        // Throw from outfield arrives at ~4-5s, so runner should be just ahead.
        const double minimumDuration =
            baseCount >= 3 ? 10.5 :
            baseCount == 2 ?  7.0 :
            startBase == 0 ?  3.8 : 3.2;
        duration = std::max(duration * RunnerVisualSpeedScale, minimumDuration);
    }

    for (int base = startBase; base <= finalBase; ++base) {
        const double progress = baseCount > 0
            ? static_cast<double>(base - startBase) / static_cast<double>(baseCount)
            : 0.0;
        points.push_back(runnerBasePoint(base, duration * progress));
    }
    return points;
}

void stretchRunnerPathToDuration(std::vector<AnimationPoint>& points, double durationSeconds) {
    if (points.empty() || durationSeconds <= 0.0) {
        return;
    }
    const double currentDuration = std::max(0.001, points.back().timeSeconds);
    const double scale = durationSeconds / currentDuration;
    for (AnimationPoint& point : points) {
        point.timeSeconds *= scale;
    }
}

double distanceFeet(const AnimationPoint& left, const AnimationPoint& right) {
    const double dx = right.x - left.x;
    const double dy = right.y - left.y;
    return std::sqrt(dx * dx + dy * dy);
}

double runnerTravelSeconds(int fromBase, int toBase, int runnerSpeed, bool twoOuts) {
    const std::vector<AnimationPoint> path = makeRunnerPath(fromBase, toBase, 0.0);
    double distance = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        distance += distanceFeet(path[i - 1], path[i]);
    }

    const double topSpeed = std::clamp(18.5 + static_cast<double>(runnerSpeed) * 0.13, 20.8, 31.0);
    const double acceleration = std::clamp(10.5 + static_cast<double>(runnerSpeed) * 0.09, 12.5, 21.0);
    const double accelerationTime = topSpeed / acceleration;
    const double accelerationDistance = 0.5 * acceleration * accelerationTime * accelerationTime;
    double travelTime = 0.0;
    if (distance <= accelerationDistance) {
        travelTime = std::sqrt((2.0 * distance) / acceleration);
    } else {
        travelTime = accelerationTime + (distance - accelerationDistance) / topSpeed;
    }

    const int turns = std::max(0, toBase - fromBase - 1);
    travelTime += static_cast<double>(turns) * 0.16;
    travelTime += 0.22;
    if (twoOuts) {
        travelTime = std::max(0.1, travelTime - 0.22);
    }
    return travelTime;
}

double throwArrivalSeconds(const Vector3& fieldingPoint,
                           int targetBase,
                           double fieldingTime,
                           double fielderArm) {
    const AnimationPoint from{fieldingPoint.x, fieldingPoint.y, fieldingPoint.z, 0.0};
    const AnimationPoint to = runnerBasePoint(targetBase, 0.0);
    const double throwDistance = distanceFeet(from, to);
    const double velocity = std::clamp(108.0 + (fielderArm - 128.0) * 0.42, 95.0, 145.0);
    return std::max(0.0, fieldingTime) + 0.15 + throwDistance / velocity;
}

TagPlay resolveTagChallenge(const std::string& runner,
                            int fromBase,
                            int targetBase,
                            int runnerSpeed,
                            double fielderArm,
                            const PlayResult& result,
                            bool twoOuts) {
    TagPlay tag;
    tag.base = targetBase;
    tag.runnerName = runner;
    tag.runnerArrivalTime = runnerTravelSeconds(fromBase, targetBase, runnerSpeed, twoOuts);
    const double fieldingTime = std::max(result.fielderTravelTime,
                                         result.fieldingAvailableTime);
    tag.ballArrivalTime = throwArrivalSeconds(result.fieldingPoint,
                                              targetBase,
                                              fieldingTime,
                                              fielderArm);
    tag.tagTime = 0.18;
    tag.runnerSafe = !(tag.ballArrivalTime + tag.tagTime < tag.runnerArrivalTime);
    return tag;
}

double tagOutProbability(const TagPlay& tag) {
    const double throwAheadSeconds = tag.runnerArrivalTime - (tag.ballArrivalTime + tag.tagTime);
    return std::clamp(0.38 + throwAheadSeconds * 0.22, 0.02, 0.94);
}

double tagSafeMarginSeconds(const TagPlay& tag) {
    return tag.ballArrivalTime + tag.tagTime - tag.runnerArrivalTime;
}

double timedAdvanceProbability(const TagPlay& tag,
                               double baselineAggression,
                               double floor,
                               double ceiling) {
    const double margin = tagSafeMarginSeconds(tag);
    const double timingRead = std::clamp(0.50 + tagSafeMarginSeconds(tag) * 0.62,
                                         0.02,
                                         0.98);
    double blended = baselineAggression * 0.28 + timingRead * 0.72;
    if (margin < -0.75) {
        blended *= 0.18;
    } else if (margin < -0.35) {
        blended *= 0.32;
    } else if (margin < -0.12) {
        blended *= 0.55;
    } else if (margin > 0.55) {
        blended += 0.08;
    }
    return std::clamp(blended, floor, ceiling);
}

bool runnerAttemptsTimedAdvance(const TagPlay& tag,
                                double baselineAggression,
                                double floor,
                                double ceiling,
                                Random& random) {
    return random.chance(timedAdvanceProbability(tag, baselineAggression, floor, ceiling));
}

ThrowOption makeThrowOption(const TagPlay& tag,
                            double runRisk,
                            double extraBaseRisk) {
    ThrowOption option;
    option.targetBase = tag.base;
    option.ballArrivalTime = tag.ballArrivalTime;
    option.runnerArrivalTime = tag.runnerArrivalTime;
    option.outProbability = tagOutProbability(tag);
    option.runRisk = runRisk;
    option.extraBaseRisk = extraBaseRisk;
    option.badThrowRisk = 0.03;
    option.accuracy = 1.0 - option.badThrowRisk;
    return option;
}

struct DefensiveDecisionContext {
    int inning = 1;
    bool isTop = true;
    int outs = 0;
    int offenseScore = 0;
    int defenseScore = 0;
    int basesOnHit = 0;
    double fieldingDistanceToTarget = 0.0;
    bool forcePlayAvailable = false;
};

double leverageMultiplier(const DefensiveDecisionContext& context) {
    const int scoreDiff = context.offenseScore - context.defenseScore;
    double leverage = 1.0;
    if (context.inning >= 7) {
        leverage += 0.16;
    }
    if (context.inning >= 9) {
        leverage += 0.12;
    }
    if (std::abs(scoreDiff) <= 1) {
        leverage += 0.18;
    } else if (std::abs(scoreDiff) >= 5) {
        leverage -= 0.18;
    }
    if (context.outs == 2) {
        leverage += 0.08;
    }
    return std::clamp(leverage, 0.72, 1.45);
}

double throwRiskPenalty(const ThrowOption& option,
                        const DefensiveDecisionContext& context) {
    double penalty = option.badThrowRisk * 1.8;
    if (option.targetBase == 4) {
        penalty += 0.03;
    }
    if (context.fieldingDistanceToTarget > 190.0) {
        penalty += 0.06;
    } else if (context.fieldingDistanceToTarget > 150.0) {
        penalty += 0.03;
    }
    if (context.outs == 2 && option.targetBase != 4) {
        penalty += 0.06;
    }
    return penalty;
}

std::string decisionReasonFor(const ThrowOption& option,
                              const DefensiveDecisionContext& context,
                              double score) {
    std::ostringstream out;
    if (option.targetBase == 4) {
        out << "protect run";
    } else if (context.forcePlayAvailable) {
        out << "take force";
    } else if (option.extraBaseRisk >= 0.60) {
        out << "cut lead runner";
    } else {
        out << "contain runner";
    }
    out << " pOut " << static_cast<int>(std::round(option.outProbability * 100.0))
        << "% score " << std::fixed << std::setprecision(2) << score;
    return out.str();
}

DefensiveDecision chooseDefensiveDecision(std::vector<ThrowOption> options,
                                          const DefensiveDecisionContext& context = {}) {
    DefensiveDecision decision;
    decision.options = std::move(options);
    const ThrowOption* bestOption = nullptr;
    double bestScore = -1000.0;
    const double leverage = leverageMultiplier(context);
    for (const ThrowOption& option : decision.options) {
        const double runValue = option.targetBase == 4
            ? 1.12 * option.runRisk
            : 0.52 * option.extraBaseRisk;
        const double outValue = option.outProbability * (0.88 + context.outs * 0.18);
        const double lateRunBonus = option.targetBase == 4 && context.inning >= 7
            ? 0.18 * option.runRisk
            : 0.0;
        const double score = (outValue + runValue + lateRunBonus) * leverage
            - throwRiskPenalty(option, context);
        if (score > bestScore) {
            bestScore = score;
            bestOption = &option;
        }
    }

    const double holdThreshold =
        context.outs == 2 ? 0.78 :
        context.inning >= 8 ? 0.63 : 0.58;
    if (bestOption != nullptr
        && bestOption->outProbability >= 0.22
        && bestScore >= holdThreshold) {
        decision.chosenTargetBase = bestOption->targetBase;
        decision.holdBall = false;
        decision.cutoff = (bestOption->targetBase >= 3
                           && (context.fieldingDistanceToTarget > 165.0
                               || bestOption->outProbability < 0.50));
        decision.reason = decisionReasonFor(*bestOption, context, bestScore);
        return decision;
    }

    decision.holdBall = true;
    decision.reason = bestOption == nullptr
        ? "hold ball: no throw option"
        : "hold ball: low value/risk";
    return decision;
}

const Fielder* findFielder(const DefenseAlignment& defense, FieldPosition position) {
    for (const Fielder& fielder : defense.fielders) {
        if (fielder.position == position) {
            return &fielder;
        }
    }
    return nullptr;
}

const Fielder* chooseCutoffFielder(const DefenseAlignment& defense,
                                   int targetBase,
                                   double sprayAngle) {
    if (targetBase == 4) {
        if (sprayAngle > 18.0) {
            if (const Fielder* firstBase = findFielder(defense, FieldPosition::FirstBase)) {
                return firstBase;
            }
        }
        if (sprayAngle < -8.0) {
            if (const Fielder* shortstop = findFielder(defense, FieldPosition::Shortstop)) {
                return shortstop;
            }
        }
        if (const Fielder* secondBase = findFielder(defense, FieldPosition::SecondBase)) {
            return secondBase;
        }
    }
    if (targetBase == 3) {
        if (const Fielder* shortstop = findFielder(defense, FieldPosition::Shortstop)) {
            return shortstop;
        }
    }
    if (targetBase == 2) {
        const FieldPosition preferred = sprayAngle < 0.0
            ? FieldPosition::Shortstop
            : FieldPosition::SecondBase;
        if (const Fielder* middleInfielder = findFielder(defense, preferred)) {
            return middleInfielder;
        }
    }
    return nullptr;
}

double throwDistanceToBase(const Vector3& fieldingPoint, int targetBase) {
    const AnimationPoint from{fieldingPoint.x, fieldingPoint.y, fieldingPoint.z, 0.0};
    return distanceFeet(from, runnerBasePoint(targetBase, 0.0));
}

DefensiveDecisionContext makeDecisionContext(const GameState& state,
                                             const PlayResult& result,
                                             int targetBase,
                                             int basesOnHit,
                                             bool forcePlayAvailable = false) {
    DefensiveDecisionContext context;
    context.inning = state.inning;
    context.isTop = state.isTop;
    context.outs = state.outs;
    context.offenseScore = state.isTop ? state.awayScore : state.homeScore;
    context.defenseScore = state.isTop ? state.homeScore : state.awayScore;
    context.basesOnHit = basesOnHit;
    context.forcePlayAvailable = forcePlayAvailable;
    context.fieldingDistanceToTarget = targetBase > 0
        ? throwDistanceToBase(result.fieldingPoint, targetBase)
        : 0.0;
    return context;
}

double throwDistanceBetween(const Vector3& fromPoint, const Vector3& toPoint) {
    const double dx = toPoint.x - fromPoint.x;
    const double dy = toPoint.y - fromPoint.y;
    return std::sqrt(dx * dx + dy * dy);
}

ThrowDecision makeDefensiveThrowDecision(const PlayResult& result,
                                         const DefensiveDecision& decision,
                                         const DefenseAlignment& defense) {
    ThrowDecision throwDecision;
    if (decision.holdBall || decision.chosenTargetBase == 0) {
        return throwDecision;
    }

    const int targetBase = decision.chosenTargetBase;
    throwDecision.targetBase = targetBase;
    throwDecision.targetSequence = {targetBase};

    const double throwDistance = throwDistanceToBase(result.fieldingPoint, targetBase);
    const bool longThrow = throwDistance > 170.0;
    if ((decision.cutoff || longThrow)
        && targetBase >= 2
        && targetBase <= 4) {
        if (const Fielder* cutoff = chooseCutoffFielder(defense,
                                                       targetBase,
                                                       result.battedBall.sprayAngle)) {
            throwDecision.useCutoff = true;
            throwDecision.cutoffFielderName = cutoff->name;
            throwDecision.cutoffPoint = cutoff->startPosition;
        }
    }
    return throwDecision;
}

void applyThrowAccuracy(PlayResult& result,
                        TagPlay& tag,
                        double fielderArm,
                        Random& random) {
    if (result.throwDecision.targetBase != 3
        && result.throwDecision.targetBase != 4) {
        result.throwDecision.throwAccuracy = 1.0;
        return;
    }

    double distance = throwDistanceToBase(result.fieldingPoint,
                                          result.throwDecision.targetBase);
    if (result.throwDecision.useCutoff) {
        const Vector3 target{
            runnerBasePoint(result.throwDecision.targetBase, 0.0).x,
            runnerBasePoint(result.throwDecision.targetBase, 0.0).y,
            0.0
        };
        const double firstLeg = throwDistanceBetween(result.fieldingPoint,
                                                     result.throwDecision.cutoffPoint);
        const double secondLeg = throwDistanceBetween(result.throwDecision.cutoffPoint,
                                                      target);
        distance = std::max(firstLeg, secondLeg);
    }
    const double distanceFactor = std::clamp((distance - 130.0) / 220.0, 0.0, 0.16);
    const double armFactor = std::clamp((fielderArm - 128.0) / 220.0, -0.08, 0.10);
    const double cutoffFactor = result.throwDecision.useCutoff ? -0.025 : 0.0;
    const double baseRisk = result.throwDecision.targetBase == 4 ? 0.035 : 0.025;
    const double badThrowRisk = std::clamp(baseRisk + distanceFactor - armFactor + cutoffFactor,
                                           0.01,
                                           0.18);

    const bool wasRunnerSafe = tag.runnerSafe;
    result.throwDecision.throwAccuracy = 1.0 - badThrowRisk;
    if (!random.chance(badThrowRisk)) {
        return;
    }

    result.throwDecision.badThrow = true;
    result.throwDecision.throwAccuracy = 1.0 - badThrowRisk;
    result.throwDecision.offlineFeet = random.real(4.0, 18.0);
    result.throwDecision.arrivalDelaySeconds = random.real(0.15, 0.45);
    tag.ballArrivalTime += result.throwDecision.arrivalDelaySeconds;
    tag.tagTime += 0.08 + result.throwDecision.offlineFeet * 0.01;
    tag.runnerSafe = !(tag.ballArrivalTime + tag.tagTime < tag.runnerArrivalTime);
    if (!wasRunnerSafe && tag.runnerSafe) {
        result.throwingError = true;
        result.throwingErrorFielderName = result.throwDecision.useCutoff
            ? result.throwDecision.cutoffFielderName
            : result.fielderName;
    }
}

bool badThrowAllowsExtraBase(PlayResult& result, Random& random) {
    if (!result.throwDecision.badThrow || result.throwDecision.offlineFeet < 12.0) {
        return false;
    }
    const double chance = std::clamp((result.throwDecision.offlineFeet - 10.0) * 0.035,
                                     0.05,
                                     0.35);
    if (!random.chance(chance)) {
        return false;
    }
    result.throwingError = true;
    result.badThrowExtraBase = true;
    if (result.throwingErrorFielderName.empty()) {
        result.throwingErrorFielderName = result.throwDecision.useCutoff
            ? result.throwDecision.cutoffFielderName
            : result.fielderName;
    }
    return true;
}

std::vector<RunnerAnimation> buildRunnerAnimationsFromBases(
    const std::array<std::optional<std::string>, 3>& beforeBases,
    const GameState& afterState,
    const PlayResult& result) {
    std::vector<RunnerAnimation> runners;
    if (!supportsRunnerAnimation(result.type)) {
        return runners;
    }

    std::vector<std::pair<std::string, int>> candidates;
    candidates.reserve(4);
    candidates.push_back({result.batterName, 0});
    for (int i = 0; i < 3; ++i) {
        if (beforeBases[static_cast<std::size_t>(i)].has_value()) {
            candidates.push_back({*beforeBases[static_cast<std::size_t>(i)], i + 1});
        }
    }

    for (const auto& candidate : candidates) {
        const std::string& runnerName = candidate.first;
        const int fromBase = candidate.second;
        int toBase = -1;
        for (int i = 0; i < 3; ++i) {
            if (afterState.bases[static_cast<std::size_t>(i)].has_value()
                && *afterState.bases[static_cast<std::size_t>(i)] == runnerName) {
                toBase = i + 1;
                break;
            }
        }

        const bool scored = runnerScoredOnPlay(result, runnerName);
        if (scored) {
            toBase = 4;
        }
        if (toBase < 0 || toBase == fromBase) {
            continue;
        }

        RunnerAnimation animation;
        animation.runnerName = runnerName;
        animation.fromBase = fromBase;
        animation.toBase = toBase;
        animation.scored = scored;
        const double secondsPerBase = result.type == AtBatResultType::Walk ? 4.5 : 2.5;
        animation.points = makeRunnerPath(fromBase, toBase, secondsPerBase);
        if (!animation.points.empty()) {
            animation.durationSeconds = animation.points.back().timeSeconds;
        }
        runners.push_back(animation);
    }

    // アウトになった走者のアニメーション (刺殺)
    for (const auto& runnerOut : result.runnerOuts) {
        RunnerAnimation animation;
        animation.runnerName = runnerOut.runnerName;
        animation.fromBase = runnerOut.fromBase;
        animation.toBase = runnerOut.outAtBase;
        animation.out = true;
        animation.scored = false;
        const double secondsPerBase = 2.5;
        animation.points = makeRunnerPath(runnerOut.fromBase, runnerOut.outAtBase, secondsPerBase);
        double timedArrival = 0.0;
        for (const TagPlay& tag : result.tagPlays) {
            if (tag.runnerName == runnerOut.runnerName
                && tag.base == runnerOut.outAtBase) {
                timedArrival = tag.runnerArrivalTime;
                break;
            }
        }
        if (timedArrival <= 0.0 && result.throwDecision.targetBase > 0) {
            const double fieldingTime = std::max(result.fielderTravelTime,
                                                 result.fieldingAvailableTime);
            timedArrival = throwArrivalSeconds(result.fieldingPoint,
                                               runnerOut.outAtBase,
                                               fieldingTime,
                                               128.0) + 0.23;
        }
        if (timedArrival > 0.0) {
            stretchRunnerPathToDuration(animation.points, timedArrival);
        }
        if (!animation.points.empty()) {
            animation.durationSeconds = animation.points.back().timeSeconds;
        }
        runners.push_back(animation);
    }

    return runners;
}

std::vector<RunnerAnimation> buildBaseRunningAnimations(
    const std::vector<BaseRunningEvent>& events)
{
    std::vector<RunnerAnimation> result;
    for (const auto& ev : events) {
        if (ev.runnerName.empty()) continue;
        RunnerAnimation anim;
        anim.runnerName = ev.runnerName;
        anim.fromBase   = ev.fromBase;
        anim.toBase     = ev.toBase; // 4 = home is valid in RunnerAnimation
        anim.scored     = ev.scored;
        anim.out        = (ev.type == BaseRunningEventType::CaughtStealing
                           || ev.type == BaseRunningEventType::PickoffOut);
        const double spb = 2.5;
        anim.points = makeRunnerPath(ev.fromBase, ev.toBase, spb);
        if (!anim.points.empty())
            anim.durationSeconds = anim.points.back().timeSeconds;
        result.push_back(anim);
    }
    return result;
}

} // namespace

std::string toString(PitcherRole role) {
    switch (role) {
        case PitcherRole::Starter:      return "SP";
        case PitcherRole::LongRelief:   return "LR";
        case PitcherRole::MiddleRelief: return "MR";
        case PitcherRole::Setup:        return "SU";
        case PitcherRole::Closer:       return "CL";
    }
    return "RP";
}

Team::Team(std::string teamName, std::vector<Player> lineup,
           std::vector<Player> rotation, std::vector<Player> bullpen,
           std::vector<Player> bench, BallparkConfig ballpark, League league)
    : name_(std::move(teamName))
    , lineup_(std::move(lineup))
    , rotation_(std::move(rotation))
    , bullpen_(std::move(bullpen))
    , bench_(std::move(bench))
    , usedBench_(bench_.size(), false)
    , homeBallpark_(std::move(ballpark))
    , league_(league) {
    if (lineup_.empty()) throw std::invalid_argument("Team lineup cannot be empty.");
    if (rotation_.empty()) throw std::invalid_argument("Team rotation cannot be empty.");
}

const BallparkConfig& Team::homeBallpark() const { return homeBallpark_; }
League Team::league() const { return league_; }

const std::string& Team::name() const { return name_; }

const std::vector<Player>& Team::lineup() const { return lineup_; }

const Player& Team::currentBatter() const {
    return lineup_.at(static_cast<std::size_t>(currentBattingIndex_));
}

const Player& Team::starter() const {
    return rotation_.at(static_cast<std::size_t>(rotationSlot_) % rotation_.size());
}

const std::vector<Player>& Team::rotation() const { return rotation_; }

void Team::setStarterSlot(int slot) {
    rotationSlot_ = slot % static_cast<int>(rotation_.size());
}

const std::vector<Player>& Team::bullpen() const { return bullpen_; }

int Team::currentBattingIndex() const { return currentBattingIndex_; }

void Team::advanceBatter() {
    currentBattingIndex_ = (currentBattingIndex_ + 1) % static_cast<int>(lineup_.size());
}

void Team::retreatBatter() {
    const int n = static_cast<int>(lineup_.size());
    currentBattingIndex_ = (currentBattingIndex_ - 1 + n) % n;
}

const Player& Team::nextBatter() const {
    const int n = static_cast<int>(lineup_.size());
    const int next = (currentBattingIndex_ + 1) % n;
    return lineup_.at(static_cast<std::size_t>(next));
}

const std::vector<Player>& Team::bench() const { return bench_; }
const std::vector<bool>& Team::usedBench() const { return usedBench_; }

bool Team::hasBenchPlayer() const {
    for (std::size_t i = 0; i < bench_.size(); ++i) {
        if (!usedBench_[i]) return true;
    }
    return false;
}

void Team::sendPinchHitter(std::size_t benchIndex, std::size_t lineupIndex) {
    lineup_.at(lineupIndex) = bench_.at(benchIndex);
    usedBench_.at(benchIndex) = true;
}

static std::vector<PlayerBoxScore> initPlayerStats(const Team& team) {
    std::vector<PlayerBoxScore> stats;
    stats.reserve(team.lineup().size());
    for (const auto& p : team.lineup()) {
        PlayerBoxScore pbs;
        pbs.name = p.name;
        pbs.position = p.position;
        stats.push_back(pbs);
    }
    return stats;
}

static PitcherBoxScore makePitcherEntry(const Player& p, bool isStarter) {
    PitcherBoxScore pbs;
    pbs.name = p.name;
    pbs.games = 1;
    pbs.gamesStarted = isStarter ? 1 : 0;
    return pbs;
}

// 現在登板中の投手のエントリを返す (なければ追加)
static PitcherBoxScore& findOrAddPitcher(std::vector<PitcherBoxScore>& stats,
                                         const Player& pitcher, bool isStarter) {
    for (auto& pbs : stats)
        if (pbs.name == pitcher.name) return pbs;
    stats.push_back(makePitcherEntry(pitcher, isStarter));
    return stats.back();
}

GameEngine::GameEngine(Team awayTeam, Team homeTeam, Random random,
                       GameRules rules, BallparkConfig ballpark,
                       AtBatEngine atBatEngine)
    : awayTeam_(std::move(awayTeam)),
      homeTeam_(std::move(homeTeam)),
      random_(std::move(random)),
      rules_(rules),
      ballpark_(ballpark),
      atBatEngine_(std::move(atBatEngine)),
      currentAwayPitcher_(awayTeam_.starter()),
      currentHomePitcher_(homeTeam_.starter()),
      awayBullpenUsed_(awayTeam_.bullpen().size(), false),
      homeBullpenUsed_(homeTeam_.bullpen().size(), false),
      awayDefense_(),
      homeDefense_(),
      awayPlayerStats_(initPlayerStats(awayTeam_)),
      homePlayerStats_(initPlayerStats(homeTeam_)) {
    // Per-game umpire zone variation
    {
        const double hBias = random_.real(-0.09, 0.09);
        const double vBias = random_.real(-0.06, 0.06);
        atBatEngine_.setZoneJudge(ZoneJudge{UmpireProfile{hBias, vBias}});
    }
    // 試合前コンディション生成 → 守備アライメント構築
    generateGameForms();
    awayDefense_ = buildDefenseAlignment(awayTeam_, currentAwayPitcher_, gameforms_);
    homeDefense_ = buildDefenseAlignment(homeTeam_, currentHomePitcher_, gameforms_);
    // 先発投手のエントリを初期化
    awayPitcherStats_.push_back(makePitcherEntry(currentAwayPitcher_, true));
    homePitcherStats_.push_back(makePitcherEntry(currentHomePitcher_, true));
}

void GameEngine::simulate(std::ostream& out) {
    printGameHeader(out);
    bool needsCommit = false;

    while (shouldContinue()) {
        simulateHalfInning(out);
        needsCommit = true;

        if (!state_.isTop && isBottomHalfOver()) {
            break;
        }

        commitHalfInning();
        needsCommit = false;
        nextHalfInning();
        initHalfInning();
    }

    if (needsCommit) {
        commitHalfInning();
    }

    finalizeGameStats();
    printFinalScore(out);
}

const GameState& GameEngine::state() const {
    return state_;
}

const std::vector<GameLog>& GameEngine::logs() const {
    return logs_;
}

const std::optional<AtBatResult>& GameEngine::lastAtBat() const {
    return lastAtBat_;
}

const std::optional<AnimationPlan>& GameEngine::latestAnimationPlan() const {
    return latestAnimationPlan_;
}

const std::string& GameEngine::awayTeamName() const { return awayTeam_.name(); }
const std::string& GameEngine::homeTeamName() const { return homeTeam_.name(); }

const TeamBoxScore& GameEngine::awayBoxScore() const {
    return awayBoxScore_;
}

const TeamBoxScore& GameEngine::homeBoxScore() const {
    return homeBoxScore_;
}

const std::vector<int>& GameEngine::awayLineScore() const {
    return awayLineScore_;
}

const std::vector<int>& GameEngine::homeLineScore() const {
    return homeLineScore_;
}

const std::vector<PlayerBoxScore>& GameEngine::awayPlayerStats() const {
    return awayPlayerStats_;
}

const std::vector<PlayerBoxScore>& GameEngine::homePlayerStats() const {
    return homePlayerStats_;
}

const std::vector<PitcherBoxScore>& GameEngine::awayPitcherStats() const {
    return awayPitcherStats_;
}

const std::vector<PitcherBoxScore>& GameEngine::homePitcherStats() const {
    return homePitcherStats_;
}

const DefenseAlignment& GameEngine::currentDefenseAlignment() const {
    return pitchingDefenseAlignment();
}

const BallparkConfig& GameEngine::ballpark() const {
    return ballpark_;
}

GameResult GameEngine::result() const {
    GameResult r;
    r.awayScore = state_.awayScore;
    r.homeScore = state_.homeScore;
    r.innings = state_.inning;
    if (state_.awayScore > state_.homeScore) {
        r.type = GameResultType::AwayWin;
    } else if (state_.homeScore > state_.awayScore) {
        r.type = GameResultType::HomeWin;
    } else {
        r.type = GameResultType::Tie;
    }
    return r;
}

int GameEngine::currentPitcherPitchCount() const {
    return state_.isTop ? state_.homePitcherPitchCount : state_.awayPitcherPitchCount;
}

void GameEngine::addPitchCount(int n) {
    if (state_.isTop) state_.homePitcherPitchCount += n;
    else              state_.awayPitcherPitchCount += n;
}

const Player& GameEngine::currentPitcher() const {
    return state_.isTop ? currentHomePitcher_ : currentAwayPitcher_;
}

const std::optional<GameEngine::PitcherChangeEvent>& GameEngine::lastPitcherChange() const {
    return lastPitcherChange_;
}

const std::vector<Player>& GameEngine::awayLineup() const { return awayTeam_.lineup(); }
const std::vector<Player>& GameEngine::homeLineup() const { return homeTeam_.lineup(); }
int GameEngine::awayBattingIndex() const { return awayTeam_.currentBattingIndex(); }
int GameEngine::homeBattingIndex() const { return homeTeam_.currentBattingIndex(); }
const std::map<std::string, int>& GameEngine::currentPitcherArsenal() const {
    return currentPitcherArsenal_;
}

double GameEngine::pitcherFormValue() const {
    auto it = gameforms_.find(currentPitcher().name);
    return it != gameforms_.end() ? it->second.pitchingForm : 1.0;
}

double GameEngine::batterFormValue(const std::string& playerName) const {
    auto it = gameforms_.find(playerName);
    return it != gameforms_.end() ? it->second.battingForm : 1.0;
}

double GameEngine::currentPitcherERA() const {
    const auto& stats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
    for (const auto& pbs : stats) {
        if (pbs.name == currentPitcher().name)
            return pbs.era();
    }
    return 0.0;
}

double GameEngine::currentPitcherVelocityDrop() const {
    // Fatigue-driven velocity drop: form 1.0 = no drop; form 0.7 = ~6 mph loss.
    // Additionally pitch count contributes: every 20 pitches past 60 costs ~0.5 mph.
    const double form = pitcherFormValue();
    const int pc = currentPitcherPitchCount();
    const double formDrop = (1.0 - form) * 20.0;
    const double countDrop = std::max(0.0, (pc - 60) * 0.03);
    return -(formDrop + countDrop);
}

Player& GameEngine::currentPitcherMut() {
    return state_.isTop ? currentHomePitcher_ : currentAwayPitcher_;
}

int& GameEngine::currentPitcherRunsAllowed() {
    return state_.isTop ? homeCurrentPitcherRunsAllowed_ : awayCurrentPitcherRunsAllowed_;
}

std::vector<bool>& GameEngine::currentBullpenUsed() {
    return state_.isTop ? homeBullpenUsed_ : awayBullpenUsed_;
}

// ── SV/HLD/BS/IR トラッカー ────────────────────────────────────────────────

// isTop == true → home が投げている → homeSit_
GameEngine::PitcherSitTracker& GameEngine::currentSit() {
    return state_.isTop ? homeSit_ : awaySit_;
}

// 投手が新たにマウンドに上がった時にセーブ状況・塁上走者を記録
void GameEngine::initPitcherSit() {
    PitcherSitTracker& sit = currentSit();
    sit.blownSave             = false;
    sit.inheritedRunnersScored = 0;
    sit.inheritedRunners.clear();
    for (const auto& b : state_.bases)
        if (b) sit.inheritedRunners.push_back(*b);

    auto& pstats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
    const PitcherBoxScore& pbs = findOrAddPitcher(pstats, currentPitcher(), false);
    sit.outsAtEntry = pbs.outsRecorded;

    // セーブ状況: 投手チームが 1〜3 点リード
    const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
    const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
    const int lead = myScore - oppScore;
    sit.inSaveSit = (lead >= 1 && lead <= 3);
}

// 投手がマウンドを降りる直前に HLD/BS/IR を確定する
void GameEngine::finalizePitcherSit() {
    PitcherSitTracker& sit = currentSit();
    auto& pstats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
    PitcherBoxScore& pbs = findOrAddPitcher(pstats, currentPitcher(), false);

    pbs.inheritedRunners      += static_cast<int>(sit.inheritedRunners.size())
                                 + sit.inheritedRunnersScored;
    pbs.inheritedRunnersScored += sit.inheritedRunnersScored;

    if (sit.inSaveSit) {
        if (sit.blownSave) {
            pbs.blownSaves++;
        } else if (pbs.outsRecorded > sit.outsAtEntry) {
            // セーブ状況で 1 アウト以上取り、リードを守ったまま降板 → Hold
            pbs.holds++;
        }
    }
    // resetは次の initPitcherSit() で行うので不要
}

// ゲーム終了時に Save / GamesFinished を確定する
void GameEngine::finalizeGameStats() {
    if (gameStatsFinalized_) return;
    gameStatsFinalized_ = true;

    const bool awayWon = state_.awayScore > state_.homeScore;
    const bool homeWon = state_.homeScore > state_.awayScore;

    // GamesFinished + Save + W (勝利チームの最終投手)
    auto finalizeLast = [&](const Player& lastPitcher,
                            PitcherSitTracker& sit,
                            std::vector<PitcherBoxScore>& pstats,
                            bool pitchingTeamWon,
                            bool starterPotentialWin) {
        PitcherBoxScore& pbs = findOrAddPitcher(pstats, lastPitcher, false);
        pbs.gamesFinished++;
        pbs.inheritedRunners      += static_cast<int>(sit.inheritedRunners.size())
                                     + sit.inheritedRunnersScored;
        pbs.inheritedRunnersScored += sit.inheritedRunnersScored;

        if (sit.inSaveSit && sit.blownSave) {
            pbs.blownSaves++;
        } else if (sit.inSaveSit && pitchingTeamWon
                   && pbs.outsRecorded > sit.outsAtEntry) {
            pbs.saves++;
        }

        if (pitchingTeamWon) {
            if (starterPotentialWin) {
                // 先発が 5 回以上投げてリードで降板 → 先発 W
                for (auto& p : pstats) {
                    if (p.gamesStarted > 0) { p.wins++; break; }
                }
            } else {
                // 先発が資格なし → 最終登板投手 (bullpen) に W
                pbs.wins++;
            }
        }
    };

    finalizeLast(currentAwayPitcher_, awaySit_, awayPitcherStats_, awayWon, awayStarterPotentialWin_);
    finalizeLast(currentHomePitcher_, homeSit_, homePitcherStats_, homeWon, homeStarterPotentialWin_);
}

// ──────────────────────────────────────────────────────────────────────────

void GameEngine::changePitcher(const Player& newPitcher, std::size_t bullpenIndex) {
    finalizePitcherSit();   // 旧投手の HLD/BS/IR を確定

    // 先発 W 資格チェック: 5 回 (15 アウト) 以上投げてリードで降板
    {
        const auto& pstats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
        for (const auto& pbs : pstats) {
            if (pbs.name == currentPitcher().name && pbs.gamesStarted > 0
                && pbs.outsRecorded >= 15) {
                const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
                const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
                if (myScore > oppScore) {
                    if (state_.isTop) homeStarterPotentialWin_ = true;
                    else              awayStarterPotentialWin_ = true;
                }
                break;
            }
        }
    }

    // 投手交代イベント: 理由を自動判定して記録 (SFML バナー用)
    {
        const std::string fromName = currentPitcher().name;
        const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
        const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
        const int lead = myScore - oppScore;
        std::string reason = "PITCHING CHANGE";
        if (newPitcher.pitcherRole == PitcherRole::Closer && lead >= 1 && lead <= 3) {
            reason = "SAVE SITUATION";
        } else if (lead >= 5) {
            reason = "BLOWOUT";
        } else if (currentPitcherPitchCount() >= 85) {
            reason = "FATIGUE";
        } else {
            double form = 1.0;
            if (auto it = gameforms_.find(currentPitcher().name); it != gameforms_.end())
                form = it->second.pitchingForm;
            if (form < 0.97) reason = "EARLY HOOK";
        }
        lastPitcherChange_ = PitcherChangeEvent{fromName, newPitcher.name, reason};
    }

    currentPitcherArsenal_.clear();  // 投手交代でアーセナルリセット
    currentBullpenUsed()[bullpenIndex] = true;
    currentPitcherMut() = newPitcher;
    auto& pstats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
    findOrAddPitcher(pstats, newPitcher, false);
    if (state_.isTop) {
        state_.homePitcherPitchCount       = 0;
        homeCurrentPitcherRunsAllowed_      = 0;
        homeDefense_ = buildDefenseAlignment(homeTeam_, currentHomePitcher_, gameforms_);
    } else {
        state_.awayPitcherPitchCount       = 0;
        awayCurrentPitcherRunsAllowed_      = 0;
        awayDefense_ = buildDefenseAlignment(awayTeam_, currentAwayPitcher_, gameforms_);
    }
    initPitcherSit();       // 新投手のセーブ状況・塁上走者を記録
}

void GameEngine::tryPitcherChange() {
    const Team& team    = state_.isTop ? homeTeam_ : awayTeam_;
    const auto& bullpen = team.bullpen();
    auto& used          = currentBullpenUsed();
    const int inning    = state_.inning;
    const int myScore   = state_.isTop ? state_.homeScore : state_.awayScore;
    const int oppScore  = state_.isTop ? state_.awayScore : state_.homeScore;
    const int lead      = myScore - oppScore;
    const bool saveSit  = lead > 0 && lead <= 3;   // セーブ状況
    const bool isClose  = std::abs(lead) <= 2;      // 接戦
    const bool bigLead  = lead >= 5;                // 大差

    // 現打者の打席側 (プラトーン用)
    const BattingSide currentBatterSide = battingTeam().currentBatter().battingSide;
    const bool wantLefty = (currentBatterSide == BattingSide::Left
                         || currentBatterSide == BattingSide::Switch);

    // 接戦: 同役割の中から最も球威+コントロールが高い投手を選ぶ。
    // platoon=true のとき、打者の打席側と逆投手に +8 ボーナスを与える。
    auto tryBestOfRole = [&](PitcherRole role, bool platoon = false) -> bool {
        std::size_t best = bullpen.size();
        int bestQ = -1;
        for (std::size_t i = 0; i < bullpen.size(); ++i) {
            if (used[i] || bullpen[i].pitcherRole != role) continue;
            int q = bullpen[i].pitchingStuff + bullpen[i].pitchingControl;
            if (platoon) {
                const bool isLefty = (bullpen[i].throwingHand == ThrowingHand::Left);
                if (isLefty == wantLefty) q += 8; // 手の向きが有利 → ボーナス
            }
            if (q > bestQ) { bestQ = q; best = i; }
        }
        if (best < bullpen.size()) { changePitcher(bullpen[best], best); return true; }
        return false;
    };

    // 先着順で役割一致を探す (通常用)
    auto tryRole = [&](PitcherRole role) -> bool {
        for (std::size_t i = 0; i < bullpen.size(); ++i) {
            if (!used[i] && bullpen[i].pitcherRole == role) {
                changePitcher(bullpen[i], i); return true;
            }
        }
        return false;
    };

    // 9回以降+セーブ → Closer (ブロー後はフォールバック)
    if (inning >= 9 && saveSit && tryRole(PitcherRole::Closer))            return;
    // 大差 → LongRelief で温存
    if (bigLead && tryRole(PitcherRole::LongRelief))                        return;
    // 接戦 8回+ → Setup (プラトーン考慮)
    if (inning >= 8 && isClose && tryBestOfRole(PitcherRole::Setup, true))       return;
    // 接戦 6-7回 → 最良 MiddleRelief (プラトーン考慮)
    if (inning >= 6 && isClose && tryBestOfRole(PitcherRole::MiddleRelief, true)) return;
    // 通常 8回+ → Setup
    if (inning >= 8 && tryRole(PitcherRole::Setup))                        return;
    // 通常 → MiddleRelief → LongRelief の順
    if (tryRole(PitcherRole::MiddleRelief))                                 return;
    if (tryRole(PitcherRole::LongRelief))                                   return;
    // 最後: 使えるなら誰でも
    for (std::size_t i = 0; i < bullpen.size(); ++i) {
        if (!used[i]) { changePitcher(bullpen[i], i); return; }
    }
}

void GameEngine::considerPitcherChange() {
    const int pitchCount  = currentPitcherPitchCount();
    const int runsAllowed = currentPitcherRunsAllowed();
    const PitcherRole role = currentPitcher().pitcherRole;

    // 当日コンディションが悪いか
    double pitcherForm = 1.0;
    if (auto it = gameforms_.find(currentPitcher().name); it != gameforms_.end())
        pitcherForm = it->second.pitchingForm;
    const bool badForm = pitcherForm < 0.97;

    // 9回開始 + セーブ状況: Setup/Middle は Closer に交代する
    // (投球数閾値に関わらず、イニング頭でのクローザー登板機会を作る)
    if (state_.outs == 0 && state_.inning >= 9
        && (role == PitcherRole::Setup || role == PitcherRole::MiddleRelief
            || role == PitcherRole::Starter)) {
        const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
        const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
        const int lead = myScore - oppScore;
        if (lead >= 1 && lead <= 3) {
            tryPitcherChange();
            return;
        }
    }

    // スタミナ連動: pitchingStamina が高いほど交代閾値が高く、低いほど早めに引く
    const int stamina = currentPitcher().pitchingStamina;
    const int staminaBonus = (stamina - 50) / 5;  // stamina 40→-2, 50→0, 60→+2, 70→+4

    // 連投中の投手は早めに引く
    bool fatigued = false;
    if (auto it = fatigueMap_.find(currentPitcher().name); it != fatigueMap_.end())
        fatigued = (it->second <= 1);

    bool shouldChange = false;
    if (role == PitcherRole::Starter) {
        const int hardLimit = 110 + staminaBonus;
        const int softLimit = 100 + staminaBonus;
        if      (pitchCount >= hardLimit)                                        shouldChange = true;
        else if (pitchCount >= softLimit)                                        shouldChange = true;
        else if (pitchCount >= 90  && runsAllowed >= 3)                          shouldChange = true;
        else if (pitchCount >= 80  && state_.inning >= 7)                        shouldChange = true;
        else if (pitchCount >= 75  && badForm && runsAllowed >= 2)               shouldChange = true;
        else if (state_.inning >= 5 && runsAllowed >= 5)                         shouldChange = true;
    } else {
        const int relMax = fatigued ? 25 : 35;
        const int relMid = fatigued ? 18 : 25;
        const int relLow = fatigued ? 12 : 20;
        if      (pitchCount >= relMax)                                           shouldChange = true;
        else if (pitchCount >= relMid && runsAllowed >= 2)                       shouldChange = true;
        else if (pitchCount >= relLow && badForm && runsAllowed >= 1)            shouldChange = true;
    }

    if (shouldChange) tryPitcherChange();
}

// ── 代打 (Pinch Hitter) ────────────────────────────────────────
void GameEngine::considerPinchHitter() {
    if (state_.inning < 6) return;

    Team& batting = battingTeam();
    const std::size_t lineupIdx = static_cast<std::size_t>(batting.currentBattingIndex());
    const Player& current = batting.currentBatter();

    // 弱打者のみ対象
    if (current.contact >= 48) return;

    // ハイレバレッジ状況チェック
    const bool runnersOn = state_.bases[0].has_value()
                        || state_.bases[1].has_value()
                        || state_.bases[2].has_value();
    const int scoreDiff = std::abs(state_.awayScore - state_.homeScore);
    const bool highLeverage = runnersOn || (scoreDiff <= 2 && state_.inning >= 7);
    if (!highLeverage) return;

    // ベンチから最良の代打候補を探す
    const auto& bench = batting.bench();
    const auto& usedBench = batting.usedBench();
    std::size_t best = bench.size();
    int bestScore = current.contact + current.eye;
    for (std::size_t i = 0; i < bench.size(); ++i) {
        if (usedBench[i]) continue;
        const int score = bench[i].contact + bench[i].eye;
        if (score > bestScore + 15) {
            bestScore = score;
            best = i;
        }
    }
    if (best == bench.size()) return;

    const std::string originalName = current.name;
    const std::string phName = bench[best].name;
    batting.sendPinchHitter(best, lineupIdx);

    // アニメーション説明にイベントを記録
    if (!latestAnimationPlan_.has_value()) {
        latestAnimationPlan_ = AnimationPlan{};
    }
    latestAnimationPlan_->description = phName + " PH for " + originalName;
}

// ── 申告敬遠 (Intentional Walk) ────────────────────────────────
bool GameEngine::considerIntentionalWalk() {
    if (state_.inning < 7) return false;
    if (state_.bases[0].has_value()) return false;  // 1塁が埋まっている場合は申告敬遠しない
    if (state_.outs >= 2) return false;              // 2アウトは塁が埋まるだけでリスク

    const Player& batter = battingTeam().currentBatter();
    const int threatScore = batter.contact + batter.power + batter.eye;
    if (threatScore < 190) return false;  // 脅威でない打者はスキップ

    // スコア状況: 守備側のリードを計算
    const int lead = state_.isTop
        ? (state_.homeScore - state_.awayScore)
        : (state_.awayScore - state_.homeScore);
    // 申告敬遠は守備側がリードまたは1点ビハインドのときのみ有効
    if (lead < -1) return false;

    // 次打者が現打者より明らかに弱い場合のみ申告敬遠
    const Player& nextBatter = battingTeam().nextBatter();
    const int nextThreat = nextBatter.contact + nextBatter.power + nextBatter.eye;
    if (nextThreat >= threatScore - 20) return false;

    // 申告敬遠を発行
    PlayResult ibbResult;
    ibbResult.type = AtBatResultType::Walk;
    ibbResult.batterName = batter.name;
    ibbResult.pitcherName = currentPitcher().name;
    ibbResult.events.push_back("Intentional walk issued to " + batter.name + ".");

    PlayResult applied = applyPlay(ibbResult);
    logPlay(applied, nullptr);
    battingTeam().advanceBatter();
    atBatInProgress_ = false;
    pendingAtBatResult_.reset();

    // アニメーション
    AnimationPlan plan;
    plan.description = "IBB: " + batter.name;
    latestAnimationPlan_ = plan;

    return true;
}

// ── form 生成 ──────────────────────────────────────────────────
void GameEngine::generateGameForms() {
    gameforms_.clear();
    const auto genForm = [&]() {
        PlayerGameForm f;
        f.battingForm  = 0.92 + random_.real(0.0, 0.16);
        f.pitchingForm = 0.92 + random_.real(0.0, 0.16);
        f.fieldingForm = 0.92 + random_.real(0.0, 0.16);
        return f;
    };
    for (const auto& team : {std::cref(awayTeam_), std::cref(homeTeam_)}) {
        for (const auto& p : team.get().lineup())  gameforms_[p.name] = genForm();
        gameforms_[team.get().starter().name]      = genForm();
        for (const auto& p : team.get().bullpen()) gameforms_[p.name] = genForm();
    }
}

MatchupContext GameEngine::matchupContext(const Player& batter, const Player& pitcher) const {
    BattingSide side = batter.battingSide;
    if (side == BattingSide::Switch) {
        side = pitcher.throwingHand == ThrowingHand::Right ? BattingSide::Left : BattingSide::Right;
    }

    MatchupContext context;
    context.pitcherHand = pitcher.throwingHand;
    context.batterSide = side;
    context.sameHanded = (pitcher.throwingHand == ThrowingHand::Right && side == BattingSide::Right)
                      || (pitcher.throwingHand == ThrowingHand::Left && side == BattingSide::Left);
    context.oppositeHanded = !context.sameHanded;
    context.platoonAdvantage = context.oppositeHanded;
    return context;
}

// ── batting form + handedness matchup 適用 ─────────────────────
Player GameEngine::formedBatter(const Player& batter, const Player& pitcher) const {
    auto it = gameforms_.find(batter.name);
    Player p = batter;
    if (it != gameforms_.end()) {
        const double f = it->second.battingForm;
        p.contact = std::max(1, static_cast<int>(p.contact * f));
        p.power   = std::max(1, static_cast<int>(p.power   * f));
        p.eye     = std::max(1, static_cast<int>(p.eye     * f));
    }

    const MatchupContext matchup = matchupContext(batter, pitcher);
    p.battingSide = matchup.batterSide;
    if (matchup.sameHanded) {
        p.contact = std::max(1, p.contact - 3);
        p.eye = std::max(1, p.eye - 2);
    } else {
        p.contact += 2;
        p.eye += 2;
    }

    // RISP situational: runner on 2B or 3B → slightly more patient, protect the plate
    const bool risp = state_.bases[1].has_value() || state_.bases[2].has_value();
    if (risp) {
        p.eye     = std::min(80, p.eye     + 1);
        p.contact = std::min(80, p.contact + 1);
    }

    return p;
}

// ── pitching form + fatigue ────────────────────────────────────
Player GameEngine::effectivePitcher() const {
    Player p = currentPitcher();

    // 1. 試合前コンディション補正 (form)
    if (auto it = gameforms_.find(p.name); it != gameforms_.end()) {
        const double f = it->second.pitchingForm;
        p.pitchingVelocity = std::max(1, static_cast<int>(p.pitchingVelocity * f));
        p.pitchingControl  = std::max(1, static_cast<int>(p.pitchingControl  * f));
        p.pitchingStuff    = std::max(1, static_cast<int>(p.pitchingStuff    * f));
    }

    // 2. 試合内疲労補正 (fatigue) — stamina 超過分を差し引く
    const int count = currentPitcherPitchCount();
    const double penalty = std::max(0.0, static_cast<double>(count - p.pitchingStamina));
    if (penalty > 0.0) {
        const double acc = (count > 100) ? 1.5 : 1.0;
        p.pitchingVelocity = std::max(40, p.pitchingVelocity - static_cast<int>(penalty * 0.25 * acc));
        p.pitchingControl  = std::max(25, p.pitchingControl  - static_cast<int>(penalty * 0.35 * acc));
        p.pitchingStuff    = std::max(30, p.pitchingStuff    - static_cast<int>(penalty * 0.25 * acc));
    }

    // 3. 連投疲労補正 — 前日登板(0日休養)は能力ダウン、2日連続(1日休養)は軽微ダウン
    if (auto it = fatigueMap_.find(p.name); it != fatigueMap_.end()) {
        const int daysRest = it->second;
        if (daysRest == 0) {
            p.pitchingVelocity = std::max(40, p.pitchingVelocity - 4);
            p.pitchingControl  = std::max(25, p.pitchingControl  - 6);
            p.pitchingStuff    = std::max(30, p.pitchingStuff    - 4);
        } else if (daysRest == 1) {
            p.pitchingVelocity = std::max(40, p.pitchingVelocity - 2);
            p.pitchingControl  = std::max(25, p.pitchingControl  - 3);
            p.pitchingStuff    = std::max(30, p.pitchingStuff    - 2);
        }
    }
    return p;
}

void GameEngine::setFatigueMap(std::map<std::string, int> m) {
    fatigueMap_ = std::move(m);
}

bool GameEngine::simulateNextPitch() {
    if (!shouldContinue()) return false;

    // イニング切り替え（新打席の開始前に行う）
    if (!atBatInProgress_ && !pendingAtBatResult_) {
        if (state_.outs >= 3 || isWalkOffState()) {
            if (!state_.isTop && isBottomHalfOver()) {
                commitHalfInning();
                return false;
            }
            commitHalfInning();
            nextHalfInning();
            initHalfInning();
            if (!shouldContinue()) return false;
        }
    }

    // 新打席開始
    if (!atBatInProgress_ && !pendingAtBatResult_) {
        considerPitcherChange();
        considerPinchHitter();
        if (considerIntentionalWalk()) return false;
        const Player& batter   = battingTeam().currentBatter();
        const Player  effective = effectivePitcher(); // 疲労補正済み
        const Player  formed = formedBatter(batter, effective);
        battingTeam().advanceBatter();
        currentAtBat_ = atBatEngine_.startAtBat(formed, effective);
        atBatInProgress_ = true;
    }

    // 投球前イベント: ボーク、牽制、盗塁
    latestBaseRunningEvents_.clear();
    const std::size_t prevSize = currentAtBat_.baseRunningEvents.size();
    checkBalk();
    checkPickoff();
    checkStolenBase();
    // Record pre-pitch events (Balk/Pickoff/SB/CS)
    latestBaseRunningEvents_.assign(
        currentAtBat_.baseRunningEvents.begin() + static_cast<std::ptrdiff_t>(prevSize),
        currentAtBat_.baseRunningEvents.end());

    // CS/ボークで3アウトになった場合は投球せずにイニング終了へ
    // advanceBatter() 済みなので打順を1つ戻し、次イニングの先頭打者を同じ打者にする
    if (state_.outs >= 3) {
        battingTeam().retreatBatter();
        atBatInProgress_ = false;
        // Build an animation plan with just the runner movements
        AnimationPlan plan;
        plan.runners = buildBaseRunningAnimations(latestBaseRunningEvents_);
        for (const auto& r : plan.runners)
            plan.totalDurationSeconds = std::max(plan.totalDurationSeconds, r.durationSeconds);
        AnimationPlanBuilder builder;
        builder.rebuildDerivedTimeline(plan);
        latestAnimationPlan_ = plan;
        return false;
    }

    // バント意図: 状況に応じて設定 (毎球評価)
    {
        const bool r1B = state_.bases[0].has_value();
        const bool r2B = state_.bases[1].has_value();
        const bool r3B = state_.bases[2].has_value();
        // バント: 0アウト限定、スコア差・イニング・打者能力で重み付け
        const bool buntSituation = (r1B || r2B) && !r3B && state_.outs == 0;
        double buntProb = 0.0;
        if (buntSituation) {
            const int battingScore = state_.isTop ? state_.awayScore : state_.homeScore;
            const int pitchingScore = state_.isTop ? state_.homeScore : state_.awayScore;
            const int lead = battingScore - pitchingScore; // 正 = 攻撃チームがリード

            // 大差では送りバントの価値なし
            if (std::abs(lead) <= 2) {
                const int battingOrderPos = battingTeam().currentBattingIndex();
                const Player& currentBatter = battingTeam().currentBatter();
                const bool weakHitter = battingOrderPos >= 6
                                     || currentBatter.contact < 65;
                // イニング補正 (早い回はバントしにくい)
                const double inningMult = state_.inning >= 7 ? 2.0
                                        : state_.inning >= 5 ? 1.2
                                        : 0.5;
                // 1塁走者のみ (得点圏ではない) → バント価値高い
                const double situMult = (r1B && !r2B) ? 1.3 : 0.7;
                buntProb = (weakHitter ? 0.11 : 0.04) * inningMult * situMult;
                buntProb = std::min(buntProb, 0.30);
            }
        }
        currentAtBat_.buntIntent = (buntProb > 0.0 && random_.chance(buntProb));
        if (currentAtBat_.buntIntent) {
            battingBoxScore().buntAttempts += 1;
        }
    }

    // 1球進める
    bool terminal = atBatEngine_.simulateNextPitch(currentAtBat_, random_);
    addPitchCount(1);
    if (!currentAtBat_.pitchLogs.empty()) {
        currentPitcherArsenal_[joji::toString(currentAtBat_.pitchLogs.back().pitch.pitchType)]++;
    }

    // 投球後イベント: ワイルドピッチ、パスドボール
    if (!terminal) {
        const std::size_t prevSize2 = currentAtBat_.baseRunningEvents.size();
        checkWildPitchPassedBall();
        // Record post-pitch events (WP/PB)
        latestBaseRunningEvents_.insert(
            latestBaseRunningEvents_.end(),
            currentAtBat_.baseRunningEvents.begin() + static_cast<std::ptrdiff_t>(prevSize2),
            currentAtBat_.baseRunningEvents.end());
    }

    latestAnimationPlan_ = pitchOnlyAnimationPlan(currentAtBat_);

    // Attach inter-pitch runner animations to the pitch plan
    if (!latestBaseRunningEvents_.empty() && latestAnimationPlan_.has_value()) {
        const double pitchEnd = latestAnimationPlan_->totalDurationSeconds;
        for (const auto& ev : latestBaseRunningEvents_) {
            if (ev.runnerName.empty()) continue;
            RunnerAnimation anim;
            anim.runnerName = ev.runnerName;
            anim.fromBase   = ev.fromBase;
            anim.toBase     = ev.toBase;
            anim.scored     = ev.scored;
            anim.out        = (ev.type == BaseRunningEventType::CaughtStealing
                               || ev.type == BaseRunningEventType::PickoffOut);
            const double spb = 2.5;
            if (ev.type == BaseRunningEventType::StolenBase ||
                ev.type == BaseRunningEventType::CaughtStealing) {
                // SB/CS happen concurrently with pitch delivery — no time offset
                anim.points = makeRunnerPath(ev.fromBase, ev.toBase, spb);
                if (!anim.points.empty())
                    anim.durationSeconds = anim.points.back().timeSeconds;
            } else {
                // WP/PB/Balk — start after the pitch animation ends
                anim.points = makeRunnerPath(ev.fromBase, ev.toBase, spb);
                for (auto& pt : anim.points) pt.timeSeconds += pitchEnd;
                if (!anim.points.empty())
                    anim.durationSeconds = anim.points.back().timeSeconds;
            }
            latestAnimationPlan_->runners.push_back(anim);
            latestAnimationPlan_->totalDurationSeconds = std::max(
                latestAnimationPlan_->totalDurationSeconds, anim.durationSeconds);
        }
        AnimationPlanBuilder builder;
        builder.rebuildDerivedTimeline(*latestAnimationPlan_);
    }

    if (terminal) {
        pendingAtBatResult_ = buildPlayResult(currentAtBat_);
        atBatInProgress_    = false;
        return true;
    }
    return false;
}

std::optional<PlayResult> GameEngine::applyPendingAtBatResult() {
    if (!pendingAtBatResult_) return std::nullopt;

    PlayResult applied = applyPlay(*pendingAtBatResult_);
    logPlay(applied, nullptr);
    pendingAtBatResult_.reset();

    // Rebuild throw animations from actual play outcome (throwDecision is now accurate)
    if (latestAnimationPlan_.has_value()) {
        AnimationPlanBuilder builder;
        latestAnimationPlan_->throws = builder.buildThrowAnimations(applied);
        for (const auto& t : latestAnimationPlan_->throws) {
            latestAnimationPlan_->totalDurationSeconds = std::max(
                latestAnimationPlan_->totalDurationSeconds,
                t.startTimeOffset + t.durationSeconds);
        }
        builder.rebuildDerivedTimeline(*latestAnimationPlan_, &applied);
    }

    return applied;
}

bool GameEngine::isAtBatInProgress() const {
    return atBatInProgress_;
}

bool GameEngine::hasPendingAtBatResult() const {
    return pendingAtBatResult_.has_value();
}

const AtBatState& GameEngine::currentAtBat() const {
    return currentAtBat_;
}

const std::vector<BaseRunningEvent>& GameEngine::latestBaseRunningEvents() const {
    return latestBaseRunningEvents_;
}

std::optional<PlayResult> GameEngine::simulateNextPlay(std::ostream* out) {
    if (!shouldContinue()) {
        finalizeGameStats();
        return std::nullopt;
    }

    if (state_.outs >= 3 || isWalkOffState()) {
        if (!state_.isTop && isBottomHalfOver()) {
            commitHalfInning();
            finalizeGameStats();
            return std::nullopt;
        }
        commitHalfInning();
        nextHalfInning();
        initHalfInning();
        if (!shouldContinue()) {
            finalizeGameStats();
            return std::nullopt;
        }
    }

    considerPitcherChange();
    const Player& batter = battingTeam().currentBatter();
    battingTeam().advanceBatter();

    PlayResult rawResult = simulateAtBat(batter, currentPitcher());
    PlayResult appliedResult = applyPlay(rawResult);
    logPlay(appliedResult, out);
    return appliedResult;
}

bool GameEngine::isComplete() const {
    if (!shouldContinue()) return true;
    if ((state_.outs >= 3 || isWalkOffState()) && !state_.isTop && isBottomHalfOver()) {
        return true;
    }
    return false;
}

Team& GameEngine::battingTeam() {
    return state_.isTop ? awayTeam_ : homeTeam_;
}

Team& GameEngine::pitchingTeam() {
    return state_.isTop ? homeTeam_ : awayTeam_;
}

const Team& GameEngine::pitchingTeam() const {
    return state_.isTop ? homeTeam_ : awayTeam_;
}

TeamBoxScore& GameEngine::battingBoxScore() {
    return state_.isTop ? awayBoxScore_ : homeBoxScore_;
}

TeamBoxScore& GameEngine::mutableAwayBoxScore() {
    return awayBoxScore_;
}

TeamBoxScore& GameEngine::mutableHomeBoxScore() {
    return homeBoxScore_;
}

std::vector<PlayerBoxScore>& GameEngine::battingPlayerStats() {
    return state_.isTop ? awayPlayerStats_ : homePlayerStats_;
}

TeamBoxScore& GameEngine::pitchingBoxScore() {
    return state_.isTop ? homeBoxScore_ : awayBoxScore_;
}

std::vector<PlayerBoxScore>& GameEngine::pitchingPlayerStats() {
    return state_.isTop ? homePlayerStats_ : awayPlayerStats_;
}

const DefenseAlignment& GameEngine::pitchingDefenseAlignment() const {
    return state_.isTop ? homeDefense_ : awayDefense_;
}

bool GameEngine::shouldContinue() const {
    if (state_.inning < 9) {
        return true;
    }

    if (state_.isTop) {
        if (state_.inning > rules_.maxInnings) {
            return false;
        }
        return state_.inning == 9 || state_.awayScore == state_.homeScore;
    }

    if (state_.homeScore > state_.awayScore) {
        return false;
    }
    return state_.inning <= rules_.maxInnings;
}

bool GameEngine::isBottomHalfOver() const {
    if (state_.inning < 9) return false;
    return state_.homeScore != state_.awayScore
        || state_.inning >= rules_.maxInnings;
}

bool GameEngine::isWalkOffState() const {
    return state_.inning >= 9 && !state_.isTop && state_.homeScore > state_.awayScore;
}

void GameEngine::initHalfInning() {
    if (rules_.useExtraInningTiebreaker && state_.inning >= rules_.tiebreakerStartInning) {
        state_.bases[1] = "Automatic Runner";
    }
}

void GameEngine::commitHalfInning() {
    const int idx = state_.inning - 1;
    if (state_.isTop) {
        awayLineScore_.resize(static_cast<std::size_t>(idx + 1), -1);
        awayLineScore_[static_cast<std::size_t>(idx)] = currentHalfInningRuns_;
    } else {
        homeLineScore_.resize(static_cast<std::size_t>(idx + 1), -1);
        homeLineScore_[static_cast<std::size_t>(idx)] = currentHalfInningRuns_;
    }
    currentHalfInningRuns_ = 0;
}

void GameEngine::simulateHalfInning(std::ostream& out) {
    out << "\n" << inningLabel() << " - " << battingTeam().name() << " batting\n";

    while (state_.outs < 3 && !isWalkOffState()) {
        considerPitcherChange();
        const Player& batter = battingTeam().currentBatter();
        battingTeam().advanceBatter();

        PlayResult rawResult = simulateAtBat(batter, currentPitcher());
        if (state_.outs >= 3 && !rawResult.batterName.empty()
            && !rawResult.events.empty()
            && rawResult.events.front().find("No pitch") != std::string::npos) {
            break;
        }
        PlayResult appliedResult = applyPlay(rawResult);
        logPlay(appliedResult, &out);
    }
}

PlayResult GameEngine::buildPlayResult(const AtBatState& atBat) {
    PlayResult result;
    result.batterName = atBat.batter.name;
    result.pitcherName = atBat.pitcher.name;

    const AtBatOutcome outcome = atBat.finalOutcome.value_or(AtBatOutcome::StrikeOut);

    // lastAtBat_ を AtBatResult 形式で記録
    AtBatResult ar = toAtBatResult(atBat);
    lastAtBat_ = ar;

    switch (outcome) {
        case AtBatOutcome::Walk:
            result.type = AtBatResultType::Walk;
            latestAnimationPlan_ = pitchOnlyAnimationPlan(ar);
            return result;
        case AtBatOutcome::HitByPitch:
            result.type = AtBatResultType::HitByPitch;
            latestAnimationPlan_ = pitchOnlyAnimationPlan(ar);
            return result;
        case AtBatOutcome::StrikeOut:
            result.type = AtBatResultType::StrikeOut;
            latestAnimationPlan_ = pitchOnlyAnimationPlan(ar);
            return result;
        case AtBatOutcome::InPlay:
            break;
    }

    if (!atBat.battedBallInput.has_value()) {
        result.type = AtBatResultType::StrikeOut;
        latestAnimationPlan_ = pitchOnlyAnimationPlan(ar);
        return result;
    }

    result.battedBall = ballPhysicsEngine_.simulate(*atBat.battedBallInput, ballpark_, fastMode_);
    result.battedBall.batterSpeedNorm = std::clamp((atBat.batter.speed - 50) / 50.0, -0.8, 0.8);
    const DefenseAlignment shiftedDefense = applyDefensiveShift(pitchingDefenseAlignment(), atBat.batter);
    const PlayResolution resolution = playResolutionEngine_.resolve(
        result.battedBall, state_, random_, shiftedDefense, ballpark_);

    result.fieldingOutcome       = resolution.fieldingOutcome;
    result.fielderId             = resolution.fielderId;
    result.fielderName           = resolution.fielderName;
    result.madeFieldingPlay      = resolution.madeFieldingPlay;
    result.fieldingPoint         = resolution.fieldingPoint;
    result.fielderTravelTime     = resolution.fielderTravelTime;
    result.fieldingAvailableTime = resolution.fieldingAvailableTime;

    if (resolution.infieldFly) {
        result.infieldFly = true;
        result.events.push_back("Infield fly rule. Batter automatically out.");
    }

    // Error: fielder reached but misplayed → batter reaches, no hit
    if (resolution.fieldingOutcome == FieldingOutcomeType::Error) {
        result.type = AtBatResultType::Error;
    } else {
        switch (resolution.type) {
            case PlayOutcomeType::Foul:
                result.type = AtBatResultType::FlyOut;
                if (result.fielderId < 0) {
                    for (const auto& f : pitchingDefenseAlignment().fielders) {
                        if (f.position == FieldPosition::Catcher) {
                            result.fielderId = f.id;
                            result.fielderName = f.name;
                            result.madeFieldingPlay = true;
                            result.fieldingOutcome = FieldingOutcomeType::Caught;
                            result.fieldingPoint = f.startPosition;
                            result.fielderTravelTime = 0.35;
                            result.fieldingAvailableTime = 0.35;
                            break;
                        }
                    }
                }
                break;
            case PlayOutcomeType::Out:
                if (result.infieldFly) {
                    result.type = AtBatResultType::InfieldFly;
                } else if (result.battedBall.isBunt) {
                    if (resolution.description == "bunt fc") {
                        result.type = AtBatResultType::BuntFielderChoice;
                    } else {
                        // Squeeze if 3B runner present; otherwise standard sac bunt
                        const bool r3B = state_.bases[2].has_value();
                        const bool r1B = state_.bases[0].has_value();
                        const bool r2B = state_.bases[1].has_value();
                        if (r3B) {
                            result.type = AtBatResultType::SqueezeBunt;
                        } else if (r1B || r2B) {
                            result.type = AtBatResultType::SacrificeBunt;
                        } else {
                            result.type = AtBatResultType::GroundOut;
                        }
                    }
                } else {
                    result.type = result.battedBall.launchAngle < 5.0
                        ? AtBatResultType::GroundOut : AtBatResultType::FlyOut;
                }
                break;
            case PlayOutcomeType::Single:
                result.type = result.battedBall.isBunt
                    ? AtBatResultType::BuntSingle : AtBatResultType::Single;
                break;
            case PlayOutcomeType::Double:   result.type = AtBatResultType::Double;  break;
            case PlayOutcomeType::Triple:   result.type = AtBatResultType::Triple;  break;
            case PlayOutcomeType::HomeRun:  result.type = AtBatResultType::HomeRun; break;
        }
    }

    AnimationPlanBuilder animationBuilder;
    latestAnimationPlan_ = animationBuilder.build(ar, resolution, result.battedBall,
                                                   pitchingDefenseAlignment());

    return result;
}

PlayResult GameEngine::simulateAtBat(const Player& batter, const Player& /*pitcher*/) {
    const Player effective = effectivePitcher();
    const Player formed    = formedBatter(batter, effective);
    currentAtBat_ = atBatEngine_.startAtBat(formed, effective);
    atBatInProgress_ = true;

    // Full-game simulation stays fast, but still runs the same pre-pitch
    // baserunning rules as pitch-by-pitch mode. Skip inning-ending attempts here;
    // the interactive path handles those with runner-only animation.
    if (state_.outs < 2) {
        checkBalk();
        checkPickoff();
        checkStolenBase();
    }

    const bool r1B = state_.bases[0].has_value();
    const bool r2B = state_.bases[1].has_value();
    const bool r3B = state_.bases[2].has_value();
    const bool buntSituation = (r1B || r2B) && !r3B && state_.outs < 2;
    const bool weakHitter = batter.contact + batter.power < 105;
    const double buntProb = buntSituation ? (weakHitter ? 0.12 : 0.05) : 0.0;
    const bool buntIntent = buntProb > 0.0 && random_.chance(buntProb);
    if (buntIntent) {
        battingBoxScore().buntAttempts += 1;
    }

    const AtBatResult ar = atBatEngine_.simulatePlateAppearance(formed,
                                                                effective,
                                                                random_,
                                                                buntIntent);
    addPitchCount(static_cast<int>(ar.pitchLogs.size()));
    lastAtBat_ = ar;

    AtBatState state;
    state.batter        = batter;  // box score には元の stats を使う
    state.pitcher       = effective;
    state.count         = ar.finalCount;
    state.pitchLogs     = ar.pitchLogs;
    state.baseRunningEvents = currentAtBat_.baseRunningEvents;
    state.isFinished    = true;
    state.finalOutcome  = ar.finalOutcome;
    state.battedBallInput = ar.battedBallInput;

    currentAtBat_ = state;
    if (state_.outs < 3 && state.finalOutcome != AtBatOutcome::InPlay) {
        checkWildPitchPassedBall();
        state.baseRunningEvents = currentAtBat_.baseRunningEvents;
    }
    atBatInProgress_ = false;

    return buildPlayResult(state);
}

PlayResult GameEngine::applyPlay(const PlayResult& rawResult) {
    PlayResult result = rawResult;
    const auto basesBeforePlay = state_.bases;

    // Find the batter's individual stats entry.
    auto& playerStats = battingPlayerStats();
    PlayerBoxScore* batter = nullptr;
    for (auto& p : playerStats) {
        if (p.name == result.batterName) {
            batter = &p;
            break;
        }
    }

    if (result.infieldFly) {
        battingBoxScore().infieldFlies += 1;
    }

    switch (result.type) {
        case AtBatResultType::StrikeOut:
            battingBoxScore().atBats += 1;
            battingBoxScore().strikeouts += 1;
            if (batter) { batter->atBats++; batter->strikeouts++; }
            result.outsRecorded = 1;
            result.events.push_back("One out recorded.");
            break;

        case AtBatResultType::GroundOut: {
            // Error は resolve() が決定済み (FieldingOutcomeType::Error → AtBatResultType::Error)
            // ここに来るのはクリーンなゴロアウトのみ
            const bool doublePlayShape =
                result.battedBall.exitVelocity <= 104.0
                && result.battedBall.estimatedDistance <= 165.0;
            const bool triplePlayShape =
                result.battedBall.exitVelocity <= 98.0
                && result.battedBall.estimatedDistance <= 145.0;
            const bool tpEligible =
                state_.outs == 0
                && state_.bases[0].has_value()
                && state_.bases[1].has_value()
                && triplePlayShape
                && random_.chance(state_.bases[2].has_value() ? 0.035 : 0.055);
            const bool dpEligible =
                state_.bases[0].has_value()
                && state_.outs < 2
                && doublePlayShape
                && random_.chance(0.32);

            battingBoxScore().atBats += 1;
            if (batter) { batter->atBats++; }

            // フィルダースチョイス: 1塁のみに走者、2アウト未満、ダブルプレー不成立
            const bool fcEligible =
                !dpEligible && !tpEligible
                && state_.bases[0].has_value()
                && !state_.bases[1].has_value()
                && state_.outs < 2
                && random_.chance(0.28);

            if (tpEligible) {
                const std::string runnerFromSecond = *state_.bases[1];
                const std::string runnerFromFirst = *state_.bases[0];
                state_.bases[1].reset();
                state_.bases[0].reset();
                result.outsRecorded = 3;
                result.runnerOuts.push_back({runnerFromSecond, 2, 3, true});
                result.runnerOuts.push_back({runnerFromFirst, 1, 2, true});
                result.runnerOuts.push_back({result.batterName, 0, 1, true});
                result.throwDecision = {3, false, {3, 2, 1}};
                result.defensiveDecision.chosenTargetBase = 3;
                result.defensiveDecision.holdBall = false;
                result.defensiveDecision.reason = "turn triple play";
                if (batter) { batter->gidp++; }
                battingBoxScore().runnerOutsOnBases += 2;
                result.events.push_back("Ground ball — triple play. Three outs recorded.");
            } else if (dpEligible) {
                const std::string runnerFromFirst = *state_.bases[0];
                state_.bases[0].reset();
                result.outsRecorded = 2;
                result.runnerOuts.push_back({runnerFromFirst, 1, 2, true});
                result.runnerOuts.push_back({result.batterName, 0, 1, true});
                result.throwDecision = {2, true, {2, 1}}; // fielder→2B, pivot→1B
                result.defensiveDecision.chosenTargetBase = 2;
                result.defensiveDecision.holdBall = false;
                result.defensiveDecision.reason = "turn double play";
                if (batter) { batter->gidp++; }
                battingBoxScore().runnerOutsOnBases += 1;
                result.events.push_back("Ground ball — double play. Two outs recorded.");
            } else if (fcEligible) {
                // フィルダースチョイス: 守備が2塁へ送球 → 打者が1塁へ
                const std::string runner1B = *state_.bases[0];
                state_.bases[0].reset();
                state_.bases[0] = result.batterName;
                result.runnerOuts.push_back({runner1B, 1, 2, true});
                result.outsRecorded = 1;
                result.throwDecision = {2, false, {2}};
                result.defensiveDecision.chosenTargetBase = 2;
                result.defensiveDecision.holdBall = false;
                result.defensiveDecision.reason = "take force at 2B";
                result.type = AtBatResultType::FielderChoice;
                result.events.push_back("Fielder's choice. " + runner1B + " out at second.");
            } else {
                result.outsRecorded = 1;
                result.runnerOuts.push_back({result.batterName, 0, 1, true});
                result.throwDecision = {1, false, {1}}; // fielder→1B
                result.defensiveDecision.chosenTargetBase = 1;
                result.defensiveDecision.holdBall = false;
                result.defensiveDecision.reason = "routine out at 1B";
                result.events.push_back("The defense turns it into an out.");
            }
            break;
        }

        case AtBatResultType::FlyOut: {
            // Error は resolve() が決定済み — ここはクリーンなフライアウトのみ
            // 犠牲フライ: 3塁走者あり・0 or 1 アウト・外野フライ (200ft+)
            {
                const bool sacFlyEligible =
                    result.fieldingOutcome == FieldingOutcomeType::Caught
                    && state_.bases[2].has_value()
                    && state_.outs < 2
                    && result.battedBall.estimatedDistance >= 200.0;

                if (sacFlyEligible) {
                    const std::string runner = *state_.bases[2];
                    state_.bases[2].reset();

                    // 走者走力
                    int runnerSpd = 55;
                    for (const auto& p : battingTeam().lineup()) {
                        if (p.name == runner) { runnerSpd = p.speed; break; }
                    }
                    // 送球外野手の肩
                    double fArm = 128.0;
                    for (const auto& f : pitchingDefenseAlignment().fielders) {
                        if (f.name == result.fielderName) { fArm = f.armStrength; break; }
                    }
                    const double armPen  = (fArm - 128.0) * 0.006;
                    const double spdBon  = (runnerSpd - 55) * 0.003;
                    const double distBon = (result.battedBall.estimatedDistance - 280.0) * 0.0004;
                    const double pSafe   = std::clamp(0.88 - armPen + spdBon + distBon, 0.62, 0.97);

                    result.outsRecorded = 1;  // 打者は必ずアウト
                    battingBoxScore().sacFlyAttempts++;    // SFA: 常にカウント

                    TagPlay tag = resolveTagChallenge(runner, 3, 4, runnerSpd,
                                                      fArm, result, false);
                    DefensiveDecision decision = chooseDefensiveDecision(
                        {makeThrowOption(tag, 1.0, 0.10)},
                        makeDecisionContext(state_, result, 4, 0));
                    result.defensiveDecision = decision;
                    const bool runnerAttempts =
                        runnerAttemptsTimedAdvance(tag, pSafe, 0.10, 0.97, random_);
                    if (!runnerAttempts) {
                        state_.bases[2] = runner;
                        battingBoxScore().atBats += 1;
                        if (batter) { batter->atBats++; }
                        result.throwDecision = {0, false, {}};
                        result.events.push_back("Runner holds at 3B on the fly out.");
                        break;
                    }

                    if (!decision.holdBall && decision.chosenTargetBase == 4) {
                        result.throwDecision = makeDefensiveThrowDecision(result,
                                                                          decision,
                                                                          pitchingDefenseAlignment());
                        applyThrowAccuracy(result, tag, fArm, random_);
                        result.tagPlays.push_back(tag);
                    } else {
                        result.throwDecision = {0, false, {}};
                    }
                    const bool runnerSafe = decision.holdBall || tag.runnerSafe;

                    if (runnerSafe) {
                        // 犠牲フライ成立: AB にカウントしない
                        scoreRunner(result, runner);
                        battingBoxScore().sacFlySuccesses++;   // SFS
                        if (batter) { batter->sacFlies++; batter->rbi += result.runsScored; }
                        result.events.push_back("Sacrifice fly. Runner tags up and scores.");
                    } else {
                        // タッチアップ失敗: 本塁刺殺 (打者 AB カウント)
                        battingBoxScore().atBats += 1;
                        if (batter) { batter->atBats++; }
                        result.outsRecorded = 2;
                        battingBoxScore().runnerOutsOnBases++;
                        battingBoxScore().runnersThrownOutAtHome++;
                        recordThrowOutAtHome(pitchingPlayerStats(), result.fielderName);
                        result.runnerOuts.push_back({runner, 3, 4, false});
                        result.events.push_back("Runner thrown out at home trying to tag up!");
                    }
                } else {
                    battingBoxScore().atBats += 1;
                    if (batter) { batter->atBats++; }
                    result.outsRecorded = 1;
                    result.events.push_back("The ball is caught in the air.");
                }

                // 2塁走者 → 3塁タグアップ (深いフライ・3B空き・まだアウト前)
                // state_.outs < 2 はタグアップ前 (このアウトでちょうど2アウト目になる可能性も含む)
                if (state_.bases[1].has_value()
                    && !state_.bases[2].has_value()
                    && result.battedBall.estimatedDistance >= 195.0
                    && state_.outs < 2) {
                    const std::string tagRunner = *state_.bases[1];
                    int runnerSpd = 55;
                    for (const auto& p : battingTeam().lineup()) {
                        if (p.name == tagRunner) { runnerSpd = p.speed; break; }
                    }
                    const double depthBon = (result.battedBall.estimatedDistance - 195.0) * 0.0025;
                    const double spdBon2  = (runnerSpd - 55) * 0.004;
                    const double pTag     = std::clamp(0.50 + depthBon + spdBon2, 0.25, 0.88);
                    double fArm = 128.0;
                    for (const auto& f : pitchingDefenseAlignment().fielders) {
                        if (f.name == result.fielderName) { fArm = f.armStrength; break; }
                    }
                    TagPlay tag = resolveTagChallenge(tagRunner, 2, 3, runnerSpd,
                                                      fArm, result, false);
                    DefensiveDecision decision = chooseDefensiveDecision(
                        {makeThrowOption(tag, 0.20, 0.68)},
                        makeDecisionContext(state_, result, 3, 0));
                    result.defensiveDecision = decision;
                    if (runnerAttemptsTimedAdvance(tag, pTag, 0.06, 0.92, random_)) {
                        if (!decision.holdBall && decision.chosenTargetBase == 3) {
                            result.throwDecision = makeDefensiveThrowDecision(result,
                                                                              decision,
                                                                              pitchingDefenseAlignment());
                            applyThrowAccuracy(result, tag, fArm, random_);
                            result.tagPlays.push_back(tag);
                        }
                        state_.bases[1].reset();
                        if (decision.holdBall || tag.runnerSafe) {
                            const bool extraBase = !decision.holdBall
                                && tag.runnerSafe
                                && badThrowAllowsExtraBase(result, random_);
                            if (extraBase) {
                                scoreRunner(result, tagRunner);
                                result.events.push_back(tagRunner + " scores as the throw gets away!");
                            } else {
                                state_.bases[2] = tagRunner;
                                result.events.push_back(tagRunner + " tags up and advances to 3B.");
                            }
                            battingBoxScore().extraBasesTaken++;
                        } else {
                            result.outsRecorded += 1;
                            battingBoxScore().runnerOutsOnBases++;
                            result.runnerOuts.push_back({tagRunner, 2, 3, false});
                            result.events.push_back(tagRunner + " is thrown out trying to tag up to 3B.");
                        }
                    }
                }
            }
            break;
        }
        case AtBatResultType::Walk:
            battingBoxScore().walks += 1;
            if (batter) { batter->walks++; }
            advanceRunnersForWalk(result);
            break;
        case AtBatResultType::HitByPitch:
            battingBoxScore().walks += 1;  // OBP counts same as walk
            if (batter) { batter->hitByPitch++; batter->walks++; }
            advanceRunnersForWalk(result);
            result.events.push_back(result.batterName + " is hit by a pitch.");
            break;
        case AtBatResultType::Single:
            battingBoxScore().atBats += 1;
            battingBoxScore().hits += 1;
            battingBoxScore().totalBases += 1;
            advanceRunnersForHit(result, 1);
            if (batter) { batter->atBats++; batter->hits++; batter->totalBases++; batter->rbi += result.runsScored; }
            break;
        case AtBatResultType::Double:
            battingBoxScore().atBats += 1;
            battingBoxScore().hits += 1;
            battingBoxScore().totalBases += 2;
            battingBoxScore().doubles_ += 1;
            advanceRunnersForHit(result, 2);
            if (batter) { batter->atBats++; batter->hits++; batter->doubles++; batter->totalBases += 2; batter->rbi += result.runsScored; }
            break;
        case AtBatResultType::Triple:
            battingBoxScore().atBats += 1;
            battingBoxScore().hits += 1;
            battingBoxScore().totalBases += 3;
            advanceRunnersForHit(result, 3);
            if (batter) { batter->atBats++; batter->hits++; batter->triples++; batter->totalBases += 3; batter->rbi += result.runsScored; }
            break;
        case AtBatResultType::HomeRun:
            battingBoxScore().atBats += 1;
            battingBoxScore().hits += 1;
            battingBoxScore().homeRuns += 1;
            battingBoxScore().totalBases += 4;
            advanceRunnersForHit(result, 4);
            if (batter) { batter->atBats++; batter->hits++; batter->homeRuns++; batter->totalBases += 4; batter->rbi += result.runsScored; }
            break;

        case AtBatResultType::Error:
            // resolve() が FieldingOutcomeType::Error を返した場合にここへ到達
            // 打者 AB カウント・出塁 (ヒットにはならない) ・エラー記録
            battingBoxScore().atBats += 1;
            pitchingBoxScore().errors += 1;
            if (batter) { batter->atBats++; batter->reachedOnError++; }
            result.outsRecorded = 0;  // 打者はアウトでない (走者刺殺は advanceRunnersForError で加算)
            result.events.push_back("Error! Batter reaches base.");
            advanceRunnersForError(result);
            break;

        case AtBatResultType::FielderChoice:
            // フィルダースチョイス: atBats / runnerOuts / throwDecision は GroundOut case で設定済み
            // ここには来ない (type が途中で書き換わる前に GroundOut case が break する)
            // 念のため: もし直接来た場合のフォールバック
            battingBoxScore().atBats += 1;
            if (batter) { batter->atBats++; }
            result.outsRecorded = 1;
            result.events.push_back("Fielder's choice.");
            break;

        case AtBatResultType::InfieldFly:
            // インフィールドフライ: 打者自動アウト、走者は進塁義務なし
            battingBoxScore().atBats += 1;
            if (batter) { batter->atBats++; }
            result.outsRecorded = 1;
            // events already contains "Infield fly rule." from buildPlayResult
            break;

        case AtBatResultType::SacrificeBunt: {
            // 犠打: 打者アウト、走者1塁ずつ進塁、AB にカウントしない
            battingBoxScore().buntSuccesses += 1;
            result.outsRecorded = 1;
            battingBoxScore().sacBunts += 1;
            if (batter) { batter->sacBunts++; }
            // Advance runners one base (same as walk logic without batter going to 1B)
            for (int i = 2; i >= 0; --i) {
                const auto idx = static_cast<std::size_t>(i);
                if (!state_.bases[idx].has_value()) continue;
                const std::string runner = *state_.bases[idx];
                state_.bases[idx].reset();
                const int destination = i + 1;
                if (destination >= 3) {
                    scoreRunner(result, runner);
                    if (batter) { batter->rbi += 1; }
                } else {
                    state_.bases[static_cast<std::size_t>(destination)] = runner;
                    result.events.push_back(runner + " advances to "
                        + std::to_string(destination + 1)
                        + (destination == 0 ? "st." : destination == 1 ? "nd." : "rd."));
                }
            }
            result.runnerOuts.push_back({result.batterName, 0, 1, true});
            result.throwDecision = {1, false, {1}};
            result.events.push_back("Sacrifice bunt. Batter out, runners advance.");
            break;
        }

        case AtBatResultType::SqueezeBunt: {
            // スクイズ: 3B走者が生還、打者アウト、AB なし
            battingBoxScore().buntSuccesses += 1;
            result.outsRecorded = 1;
            battingBoxScore().sacBunts += 1;
            if (batter) { batter->sacBunts++; }
            // SacrificeBunt と同じ走者ループ (3B走者が必ず生還)
            for (int i = 2; i >= 0; --i) {
                const auto idx = static_cast<std::size_t>(i);
                if (!state_.bases[idx].has_value()) continue;
                const std::string runner = *state_.bases[idx];
                state_.bases[idx].reset();
                const int destination = i + 1;
                if (destination >= 3) {
                    scoreRunner(result, runner);
                    if (batter) { batter->rbi += 1; }
                } else {
                    state_.bases[static_cast<std::size_t>(destination)] = runner;
                    result.events.push_back(runner + " advances to "
                        + std::to_string(destination + 1)
                        + (destination == 0 ? "st." : destination == 1 ? "nd." : "rd."));
                }
            }
            result.runnerOuts.push_back({result.batterName, 0, 1, true});
            result.throwDecision = {1, false, {1}};
            result.events.push_back("Squeeze bunt! Runner scores.");
            break;
        }

        case AtBatResultType::BuntSingle:
            // バント安打: AB カウント・ヒット記録、走者1塁ずつ進塁
            battingBoxScore().buntSuccesses += 1;
            battingBoxScore().atBats += 1;
            battingBoxScore().hits += 1;
            battingBoxScore().totalBases += 1;
            advanceRunnersForHit(result, 1);
            if (batter) { batter->atBats++; batter->hits++; batter->totalBases++; batter->rbi += result.runsScored; }
            result.throwDecision = {1, false, {1}}; // 1B送球 (バット間に合わず)
            break;

        case AtBatResultType::BuntFielderChoice: {
            // バントFC: 守備が先の走者を狙う、打者1B出塁
            battingBoxScore().buntSuccesses += 1;
            const std::string runner1B = state_.bases[0].has_value()
                                         ? *state_.bases[0] : "";
            if (!runner1B.empty()) {
                state_.bases[0].reset();
                result.runnerOuts.push_back({runner1B, 1, 2, true});
                result.events.push_back("Bunt fielder's choice. " + runner1B + " out at second.");
            }
            state_.bases[0] = result.batterName;
            result.outsRecorded = 1;
            result.throwDecision = {2, false, {2}}; // 2B送球
            battingBoxScore().atBats += 1;
            if (batter) { batter->atBats++; }
            result.events.push_back("Batter reaches on fielder's choice.");
            break;
        }
    }

    recordDefensivePlay(result);
    recordPitchingPlay(result);
    state_.outs += result.outsRecorded;
    if (state_.isTop) {
        mutableAwayBoxScore().runs = state_.awayScore;
    } else {
        mutableHomeBoxScore().runs = state_.homeScore;
    }

    if (latestAnimationPlan_.has_value()) {
        AnimationPlanBuilder builder;
        latestAnimationPlan_->runners = buildRunnerAnimationsFromBases(basesBeforePlay, state_, result);
        for (const RunnerAnimation& runner : latestAnimationPlan_->runners) {
            latestAnimationPlan_->totalDurationSeconds = std::max(
                latestAnimationPlan_->totalDurationSeconds,
                runner.durationSeconds);
        }
        latestAnimationPlan_->throws = builder.buildThrowAnimations(result);
        for (const ThrowAnimation& throwAnimation : latestAnimationPlan_->throws) {
            latestAnimationPlan_->totalDurationSeconds = std::max(
                latestAnimationPlan_->totalDurationSeconds,
                throwAnimation.startTimeOffset + throwAnimation.durationSeconds);
        }
        builder.rebuildDerivedTimeline(*latestAnimationPlan_, &result);
    }

    return result;
}

void GameEngine::advanceRunnersForHit(PlayResult& result, int bases) {
    // 送球した外野手の肩の強さ (arm強い = 走者刺しやすい)
    double fielderArm = 128.0;  // LF/RF 平均 (ft, applyPlayerStats 後)
    for (const auto& f : pitchingDefenseAlignment().fielders) {
        if (f.name == result.fielderName) { fielderArm = f.armStrength; break; }
    }

    auto getRunnerSpeed = [&](const std::string& name) -> int {
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == name) return p.speed;
        }
        return 55;
    };

    // Track whether the fielder made one contested throw on the play.
    int contestedThrowBase = 0;

    for (int i = 2; i >= 0; --i) {
        if (!state_.bases[static_cast<std::size_t>(i)].has_value()) continue;

        const std::string runner = *state_.bases[static_cast<std::size_t>(i)];
        state_.bases[static_cast<std::size_t>(i)].reset();
        int destination = i + bases;
        bool thrownOut = false;

        // 2アウト時は走者が打球と同時にスタート → より積極的
        const bool twoOuts = (state_.outs >= 2);
        // スプレー角: 右方向(正) → 外野手が遠い → 走者有利
        // 左方向(負) → 外野手が近い → 走者不利
        const double spray    = result.battedBall.sprayAngle;
        const double sprayBon = std::clamp(spray * 0.003, -0.08, 0.08);

        if (bases == 1) {
            if (i == 1) {
                // 2塁走者: 単打でホームへ突っ込むか → 走力・肩・スプレー角の勝負
                const int spd = getRunnerSpeed(runner);
                const double armPen    = (fielderArm - 128.0) * 0.007;
                const double spdBon    = (spd - 55) * 0.006;
                // 2アウトなら迷わず走る
                const double pScoreBase = twoOuts ? 0.82 : 0.60;
                const double pScore     = std::clamp(pScoreBase + spdBon - armPen + sprayBon, 0.30, 0.92);
                TagPlay tag = resolveTagChallenge(runner, i + 1, 4, spd,
                                                  fielderArm, result, twoOuts);
                const double pChallenge = timedAdvanceProbability(
                    tag, pScore, twoOuts ? 0.12 : 0.03, 0.97);
                if (random_.chance(pChallenge)) {
                    DefensiveDecision decision = chooseDefensiveDecision(
                        {makeThrowOption(tag, 1.0, 0.25)},
                        makeDecisionContext(state_, result, 4, bases));
                    result.defensiveDecision = decision;
                    if (!decision.holdBall && decision.chosenTargetBase == 4) {
                        contestedThrowBase = 4;
                        result.throwDecision = makeDefensiveThrowDecision(result,
                                                                          decision,
                                                                          pitchingDefenseAlignment());
                        applyThrowAccuracy(result, tag, fielderArm, random_);
                        result.tagPlays.push_back(tag);
                    }
                    if (decision.holdBall || tag.runnerSafe) {
                        destination = 3;    // 生還
                        battingBoxScore().extraBasesTaken++;
                    } else {
                        thrownOut = true;   // 本塁刺殺
                    }
                } else {
                    destination = 2;    // 3塁止まり (送球なし)
                }
            } else if (i == 0) {
                // 1塁走者: 単打で3塁まで行くか → 走力・スプレー角 (刺殺リスクなし)
                const int spd = getRunnerSpeed(runner);
                const double spdBon  = (spd - 55) * 0.007;
                const double pThird  = twoOuts
                    ? std::clamp(0.55 + spdBon + sprayBon, 0.25, 0.82)
                    : std::clamp(0.31 + spdBon + sprayBon * 0.5, 0.10, 0.62);
                TagPlay tag = resolveTagChallenge(runner, i + 1, 3, spd,
                                                  fielderArm, result, twoOuts);
                const double pChallenge = timedAdvanceProbability(
                    tag, pThird, twoOuts ? 0.08 : 0.02, 0.90);
                if (random_.chance(pChallenge)) {
                    if (contestedThrowBase == 0) {
                        DefensiveDecision decision = chooseDefensiveDecision(
                            {makeThrowOption(tag, 0.25, 0.72)},
                            makeDecisionContext(state_, result, 3, bases));
                        result.defensiveDecision = decision;
                        if (!decision.holdBall && decision.chosenTargetBase == 3) {
                            contestedThrowBase = 3;
                            result.throwDecision = makeDefensiveThrowDecision(result,
                                                                              decision,
                                                                              pitchingDefenseAlignment());
                            applyThrowAccuracy(result, tag, fielderArm, random_);
                            result.tagPlays.push_back(tag);
                        }
                        if (decision.holdBall || tag.runnerSafe) {
                            const bool extraBase = !decision.holdBall
                                && tag.runnerSafe
                                && badThrowAllowsExtraBase(result, random_);
                            destination = extraBase ? 3 : 2;
                            battingBoxScore().extraBasesTaken++;
                            if (extraBase) {
                                result.events.push_back(runner + " scores as the throw gets away!");
                            }
                        } else {
                            thrownOut = true;
                        }
                    } else {
                        destination = 2;
                        battingBoxScore().extraBasesTaken++;
                    }
                }
            }
        } else if (bases == 2) {
            if (i == 0) {
                // 1塁走者: 二塁打でホームへ突っ込むか → 走力・肩・スプレー角
                const int spd = getRunnerSpeed(runner);
                const double armPen    = (fielderArm - 128.0) * 0.007;
                const double spdBon    = (spd - 55) * 0.006;
                const double pScoreBase = twoOuts ? 0.78 : 0.52;
                const double pScore     = std::clamp(pScoreBase + spdBon - armPen + sprayBon, 0.28, 0.90);
                TagPlay tag = resolveTagChallenge(runner, i + 1, 4, spd,
                                                  fielderArm, result, twoOuts);
                const double pChallenge = timedAdvanceProbability(
                    tag, pScore, twoOuts ? 0.10 : 0.03, 0.95);
                if (random_.chance(pChallenge)) {
                    DefensiveDecision decision = chooseDefensiveDecision(
                        {makeThrowOption(tag, 1.0, 0.30)},
                        makeDecisionContext(state_, result, 4, bases));
                    result.defensiveDecision = decision;
                    if (!decision.holdBall && decision.chosenTargetBase == 4) {
                        contestedThrowBase = 4;
                        result.throwDecision = makeDefensiveThrowDecision(result,
                                                                          decision,
                                                                          pitchingDefenseAlignment());
                        applyThrowAccuracy(result, tag, fielderArm, random_);
                        result.tagPlays.push_back(tag);
                    }
                    if (decision.holdBall || tag.runnerSafe) {
                        destination = 3;
                        battingBoxScore().extraBasesTaken++;
                    } else {
                        thrownOut = true;
                    }
                } else {
                    destination = 2;    // 3塁止まり
                }
            }
        }

        if (thrownOut) {
            result.outsRecorded += 1;
            battingBoxScore().runnerOutsOnBases++;
            if (contestedThrowBase == 4) {
                battingBoxScore().runnersThrownOutAtHome++;
                result.events.push_back(runner + " is thrown out at home!");
                recordThrowOutAtHome(pitchingPlayerStats(), result.fielderName);
            } else {
                result.events.push_back(runner + " is thrown out at "
                    + std::to_string(contestedThrowBase) + "B!");
            }
            result.runnerOuts.push_back({runner, i + 1, contestedThrowBase, false});
            continue;
        }

        if (destination >= 3) {
            scoreRunner(result, runner);
        } else {
            state_.bases[static_cast<std::size_t>(destination)] = runner;
            result.events.push_back(runner + " advances to " + std::to_string(destination + 1) + (destination == 0 ? "st." : destination == 1 ? "nd." : "rd."));
        }
    }

    if (bases >= 4) {
        scoreRunner(result, result.batterName);
        result.throwDecision = {0, false, {}}; // HR — no throw
        return;
    }

    state_.bases[static_cast<std::size_t>(bases - 1)] = result.batterName;
    result.events.push_back(result.batterName + " reaches " + std::to_string(bases) + (bases == 1 ? "st." : bases == 2 ? "nd." : "rd."));

    // Set throw decision based on whether a timed tag challenge occurred.
    if (contestedThrowBase != 0) {
        if (result.throwDecision.targetBase == 0) {
            result.throwDecision = makeDefensiveThrowDecision(result,
                                                              result.defensiveDecision,
                                                              pitchingDefenseAlignment());
        }
    } else {
        // No contested play at home: throw to hold lead runner / visual relay
        // Single (bases=1) → throw to 2B; Double/Triple → throw to 3B
        const int targetBase = bases == 1 ? 2 : 3;
        if (result.defensiveDecision.holdBall && !result.defensiveDecision.reason.empty()) {
            result.throwDecision = {0, false, {}};
        } else {
            result.defensiveDecision.chosenTargetBase = targetBase;
            result.defensiveDecision.holdBall = false;
            result.defensiveDecision.reason = "relay to lead base";
            result.throwDecision = makeDefensiveThrowDecision(result,
                                                              result.defensiveDecision,
                                                              pitchingDefenseAlignment());
        }
    }
}

void GameEngine::advanceRunnersForError(PlayResult& result) {
    // エラー時: 走者は通常より1塁余分に進む。大きいエラーは+2塁。
    // 本塁突入は arm/speed の TagPlay で判定 (ただしエラー後は送球品質低下)。
    const bool bigError = random_.chance(0.35);
    const bool twoOuts  = (state_.outs >= 2);

    // エラー後の送球は腕力 65% 相当に低下 (ボールの回収・体勢立て直しのため)
    double fielderArm = 128.0;
    for (const auto& f : pitchingDefenseAlignment().fielders) {
        if (f.name == result.fielderName) {
            fielderArm = f.armStrength * 0.65;
            break;
        }
    }

    auto getRunnerSpeed = [&](const std::string& name) -> int {
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == name) return p.speed;
        }
        return 55;
    };

    for (int i = 2; i >= 0; --i) {
        const auto idx = static_cast<std::size_t>(i);
        if (!state_.bases[idx].has_value()) continue;

        const std::string runner = *state_.bases[idx];
        state_.bases[idx].reset();
        const int spd = getRunnerSpeed(runner);
        int destination = i + (bigError ? 2 : 1);
        bool thrownOut = false;

        // bigError でない場合、走力で更に1塁余分に進む可能性
        if (!bigError) {
            const double spdBon = (spd - 55) * 0.008;
            if (random_.chance(std::clamp(0.25 + spdBon, 0.08, 0.55))) {
                destination += 1;
                battingBoxScore().extraBasesTaken++;
            }
        }

        // 本塁突入: 走者がホームを狙う場合に TagPlay で刺殺を評価
        // エラー後は混乱があるため送球確率は低い (3B走者: 22%、2B走者: 35%)
        if (destination >= 3) {
            TagPlay tag = resolveTagChallenge(runner, i + 1, 4, spd,
                                              fielderArm, result, twoOuts);
            const double pThrow = (i == 2) ? 0.22 : 0.35;
            if (random_.chance(pThrow) && !tag.runnerSafe) {
                thrownOut = true;
                result.tagPlays.push_back(tag);
                result.outsRecorded += 1;
                battingBoxScore().runnerOutsOnBases++;
                battingBoxScore().runnersThrownOutAtHome++;
                recordThrowOutAtHome(pitchingPlayerStats(), result.fielderName);
                result.runnerOuts.push_back({runner, i + 1, 4, false});
                result.events.push_back(runner + " is thrown out at home on the error!");
            }
        }

        if (thrownOut) continue;

        if (destination >= 3) {
            scoreRunner(result, runner);
        } else {
            state_.bases[static_cast<std::size_t>(destination)] = runner;
            result.events.push_back(runner + " advances to "
                + std::to_string(destination + 1)
                + (destination == 0 ? "st." : destination == 1 ? "nd." : "rd."));
        }
    }

    // 打者の出塁: 大きいエラーなら2B、通常は1B
    if (bigError) {
        state_.bases[1] = result.batterName;
        result.events.push_back(result.batterName + " reaches 2nd on the error.");
        battingBoxScore().extraBasesTaken++;
    } else {
        state_.bases[0] = result.batterName;
    }
}

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
    // 3塁走者がいればホームに生還
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

    // 牽制対象: 1B優先、次に2B (毎投球1回まで)
    for (int baseIdx = 0; baseIdx <= 1; ++baseIdx) {
        if (!state_.bases[static_cast<std::size_t>(baseIdx)].has_value()) continue;
        const std::string runner = *state_.bases[static_cast<std::size_t>(baseIdx)];
        const int baseNum = baseIdx + 1;

        // 試み確率: 1Bは 2%、2Bは 0.5%
        const double attemptProb = (baseIdx == 0) ? 0.012 : 0.003;
        if (!random_.chance(attemptProb)) continue;
        battingBoxScore().pickoffAttempts++;

        const int afterPitch = static_cast<int>(currentAtBat_.pitchLogs.size());

        // 走者の足
        int runnerSpd = 55;
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == runner) { runnerSpd = p.speed; break; }
        }
        // 守備側の1Bまたは2Bの送球力
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
            // 牽制アウト
            battingBoxScore().pickoffOuts++;
            state_.bases[static_cast<std::size_t>(baseIdx)].reset();
            state_.outs += 1;
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "PO: " + runner + " picked off at " + std::to_string(baseNum) + "B!"});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::PickoffOut, runner, baseNum, baseNum, false, afterPitch});
        } else {
            // 牽制試み、走者セーフ
            logs_.push_back(GameLog{state_.inning, state_.isTop,
                "PO attempt: " + runner + " safe at " + std::to_string(baseNum) + "B."});
            currentAtBat_.baseRunningEvents.push_back({
                BaseRunningEventType::PickoffAttempt, runner, baseNum, baseNum, false, afterPitch});
        }
        break; // 1投球につき1塁のみ
    }
}

void GameEngine::checkStolenBase() {
    if (!atBatInProgress_) return;
    // Never steal on 3-0, or with bases loaded (nowhere to go)
    const Count& count = currentAtBat_.count;
    if (count.balls == 3 && count.strikes == 0) return;
    if (state_.outs >= 3) return;

    // Pitcher tempo factor: faster delivery → less time for runner to get a lead.
    // Off-speed pitches (longer delivery) → easier to steal.
    double tempoAdj = 0.0;
    if (!currentAtBat_.pitchLogs.empty()) {
        const auto& lastLog = currentAtBat_.pitchLogs.back();
        const double vel = lastLog.pitch.pitchVelocity;
        tempoAdj = -(vel - 88.0) * 0.001;  // -0.007 at 95 mph, +0.003 at 85 mph
        const PitchType pt = lastLog.pitch.pitchType;
        if (pt == PitchType::Curveball || pt == PitchType::Changeup || pt == PitchType::Splitter) {
            tempoAdj += 0.010;  // slow delivery = easier jump
        }
    }

    // Catcher arm strength (shared for all steal scenarios)
    double catcherArm = 128.0;
    for (const auto& f : pitchingDefenseAlignment().fielders) {
        if (f.position == FieldPosition::Catcher) { catcherArm = f.armStrength; break; }
    }
    const double armFactor = (catcherArm - 128.0) * 0.003;

    // Helper lambda: apply one steal attempt
    auto attemptSteal = [&](const std::string& runner, int fromBase, int toBase,
                             double baseAttemptProb, double baseSuccessProb) {
        int runnerSpd = 55;
        for (const auto& p : battingTeam().lineup()) {
            if (p.name == runner) { runnerSpd = p.speed; break; }
        }
        const double spdFactor = (runnerSpd - 55) * 0.004;
        const double prob = std::clamp(baseAttemptProb + spdFactor + tempoAdj, 0.002, 0.18);
        if (!random_.chance(prob)) return;

        const std::string baseName = (toBase == 2) ? "2B" : "3B";
        battingBoxScore().stolenBaseAttempts++;
        const double successProb = std::clamp(baseSuccessProb + spdFactor - armFactor, 0.38, 0.92);
        const bool   success     = random_.chance(successProb);
        const int    afterPitch  = static_cast<int>(currentAtBat_.pitchLogs.size());

        if (success) {
            battingBoxScore().stolenBases++;
            state_.bases[static_cast<std::size_t>(fromBase - 1)].reset();
            state_.bases[static_cast<std::size_t>(toBase   - 1)] = runner;
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

    // 状況別の盗塁試み確率補正
    {
        const int battingScoreSB  = state_.isTop ? state_.awayScore : state_.homeScore;
        const int pitchingScoreSB = state_.isTop ? state_.homeScore : state_.awayScore;
        const int leadSB = battingScoreSB - pitchingScoreSB;
        // 大差でリードなら走塁アウトリスクを避ける; 大差ビハインドは1点より複数点が必要
        if (leadSB >= 4 || leadSB <= -4) return;
    }
    const int battingScoreSB  = state_.isTop ? state_.awayScore : state_.homeScore;
    const int pitchingScoreSB = state_.isTop ? state_.homeScore : state_.awayScore;
    const int leadSB = battingScoreSB - pitchingScoreSB;

    // situMult: 終盤・接戦ほど積極的; 2アウトは投球と同時スタートで成功率UP
    double situMult = 1.0;
    if (std::abs(leadSB) <= 1 && state_.inning >= 7) situMult = 1.6; // 終盤・接戦
    else if (std::abs(leadSB) <= 1)                  situMult = 1.2; // 接戦
    else if (leadSB >= 2)                             situMult = 0.7; // リード中はリスク回避
    if (state_.outs == 2) situMult *= 1.3; // 2アウトは積極走塁

    // Double steal: 1B+2B occupied, 3B open → both runners go simultaneously
    if (on1B && on2B && !on3B) {
        // Attempt probability is lower for coordinated double steal
        const std::string r1 = *state_.bases[0];
        const std::string r2 = *state_.bases[1];
        int spd1 = 55;
        for (const auto& p : battingTeam().lineup())
            if (p.name == r1) { spd1 = p.speed; break; }
        const double spdFactor1 = (spd1 - 55) * 0.004;
        if (random_.chance(std::clamp(0.039 * situMult + spdFactor1 + tempoAdj, 0.002, 0.18))) {
            const int afterPitch = static_cast<int>(currentAtBat_.pitchLogs.size());
            battingBoxScore().stolenBaseAttempts += 2;
            // Lead runner (2B→3B) resolves first; if caught, trail runner may abort
            int spd2 = 55;
            for (const auto& p : battingTeam().lineup())
                if (p.name == r2) { spd2 = p.speed; break; }
            const double spdF2 = (spd2 - 55) * 0.004;
            const bool leadSuccess = random_.chance(std::clamp(0.73 + spdF2 - armFactor, 0.38, 0.90));
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

    // Single steal: 1B→2B (most common; 2B must be open)
    if (on1B && !on2B) {
        attemptSteal(*state_.bases[0], 1, 2, 0.066 * situMult, 0.79);
    }

    // Single steal: 2B→3B (less common; 3B must be open, 1B must be empty to avoid force-out risk)
    if (on2B && !on3B && !on1B && state_.outs < 3) {
        attemptSteal(*state_.bases[1], 2, 3, 0.033 * situMult, 0.73);
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
    const int    after  = static_cast<int>(currentAtBat_.pitchLogs.size()); // 1-indexed pitch number

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
    // advance all runners and record event per runner
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

    // 引き継いだ走者のスコア追跡
    PitcherSitTracker& sit = currentSit();
    auto it = std::find(sit.inheritedRunners.begin(), sit.inheritedRunners.end(), runnerName);
    if (it != sit.inheritedRunners.end()) {
        sit.inheritedRunnersScored++;
        sit.inheritedRunners.erase(it);
    }

    // Blown Save: セーブ状況でリードが消えた瞬間
    if (sit.inSaveSit && !sit.blownSave) {
        const int myScore  = state_.isTop ? state_.homeScore : state_.awayScore;
        const int oppScore = state_.isTop ? state_.awayScore : state_.homeScore;
        if (oppScore >= myScore) sit.blownSave = true;
    }
}

void GameEngine::recordPitchingPlay(const PlayResult& result) {
    auto& pstats = state_.isTop ? homePitcherStats_ : awayPitcherStats_;
    PitcherBoxScore& p = findOrAddPitcher(pstats, currentPitcher(), false);

    p.battersFaced += 1;
    p.outsRecorded += result.outsRecorded;
    p.runsAllowed  += result.runsScored;
    p.earnedRuns   += result.runsScored; // 現在は全部自責

    switch (result.type) {
        case AtBatResultType::StrikeOut:   p.strikeouts++;        break;
        case AtBatResultType::Walk:        p.walks++;             break;
        case AtBatResultType::HomeRun:     p.homeRunsAllowed++;
                                           p.hitsAllowed++;       break;
        case AtBatResultType::Single:
        case AtBatResultType::Double:
        case AtBatResultType::Triple:      p.hitsAllowed++;       break;
        case AtBatResultType::BuntSingle:       p.hitsAllowed++; break;
        case AtBatResultType::FielderChoice:
        case AtBatResultType::BuntFielderChoice:
        case AtBatResultType::InfieldFly:
        case AtBatResultType::SacrificeBunt:
        case AtBatResultType::SqueezeBunt:
        default: break;
    }
}

void GameEngine::recordDefensivePlay(const PlayResult& result) {
    auto& defenders = pitchingPlayerStats();

    auto findByName = [&defenders](const std::string& name) -> PlayerBoxScore* {
        for (auto& player : defenders) {
            if (player.name == name) {
                return &player;
            }
        }
        return nullptr;
    };

    auto findByPosition = [&defenders](Position position) -> PlayerBoxScore* {
        for (auto& player : defenders) {
            if (player.position == position) {
                return &player;
            }
        }
        return nullptr;
    };

    PlayerBoxScore* primary = findByName(result.fielderName);
    if (result.fieldingOutcome == FieldingOutcomeType::Error) {
        if (primary != nullptr) {
            primary->errors += 1;
        }
        return;
    }

    if (result.throwingError) {
        pitchingBoxScore().errors += 1;
        pitchingBoxScore().throwingErrors += 1;
        const std::string errorFielderName = result.throwingErrorFielderName.empty()
            ? result.fielderName
            : result.throwingErrorFielderName;
        if (PlayerBoxScore* throwingFielder = findByName(errorFielderName)) {
            throwingFielder->errors += 1;
        }
    }
    if (result.throwDecision.badThrow) {
        pitchingBoxScore().badThrows += 1;
    }

    if (result.type == AtBatResultType::FlyOut) {
        if (primary != nullptr) {
            primary->putouts += 1;
        }
        return;
    }

    if (result.type == AtBatResultType::GroundOut
        || result.type == AtBatResultType::SacrificeBunt) {
        auto putoutPositionForBase = [](int base) {
            switch (base) {
                case 1: return Position::FirstBase;
                case 2: return Position::SecondBase;
                case 3: return Position::ThirdBase;
                case 4: return Position::Catcher;
                default: return Position::FirstBase;
            }
        };

        if (!result.runnerOuts.empty()) {
            if (primary != nullptr) {
                const bool unassistedPutout =
                    result.runnerOuts.size() == 1
                    && primary->position == putoutPositionForBase(result.runnerOuts.front().outAtBase);
                if (!unassistedPutout) {
                    primary->assists += 1;
                }
            }
            for (const RunnerOut& runnerOut : result.runnerOuts) {
                PlayerBoxScore* putoutFielder = findByPosition(
                    putoutPositionForBase(runnerOut.outAtBase));
                if (putoutFielder != nullptr) {
                    putoutFielder->putouts += 1;
                }
            }
        } else {
            PlayerBoxScore* firstBase = findByPosition(Position::FirstBase);
            if (primary != nullptr && primary->position == Position::FirstBase) {
                primary->putouts += 1;
            } else {
                if (primary != nullptr) {
                    primary->assists += 1;
                }
                if (firstBase != nullptr) {
                    firstBase->putouts += 1;
                }
            }
        }
    }
}

void GameEngine::clearBases() {
    for (auto& base : state_.bases) {
        base.reset();
    }
}

void GameEngine::nextHalfInning() {
    clearBases();
    state_.outs = 0;

    if (state_.isTop) {
        state_.isTop = false;
    } else {
        state_.isTop = true;
        state_.inning += 1;
    }
}

void GameEngine::logPlay(const PlayResult& result, std::ostream* out) {
    std::ostringstream line;
    line << inningLabel() << ": " << result.batterName << " " << toString(result.type) << ".";

    if (result.battedBall.estimatedDistance > 0.0) {
        line << " " << result.battedBall.classification
             << ", " << std::fixed << std::setprecision(0) << result.battedBall.estimatedDistance
             << " ft.";
    }

    if (result.fielderId >= 0) {
        line << " " << result.fielderName
             << (result.madeFieldingPlay ? " gets there." : " cannot get there.");
    }

    for (const auto& event : result.events) {
        line << " " << event;
    }

    line << " " << awayTeam_.name() << " " << state_.awayScore
         << " - " << homeTeam_.name() << " " << state_.homeScore
         << " (" << state_.outs << " out" << (state_.outs == 1 ? "" : "s") << ")";

    logs_.push_back(GameLog{state_.inning, state_.isTop, line.str()});
    if (out != nullptr) {
        *out << line.str() << "\n";
    }
}

void GameEngine::printGameHeader(std::ostream& out) const {
    out << "Joji Baseball Engine v1.0\n";
    out << awayTeam_.name() << " at " << homeTeam_.name() << "\n";
}

void GameEngine::printFinalScore(std::ostream& out) const {
    const int maxInnings = static_cast<int>(
        std::max(awayLineScore_.size(), homeLineScore_.size()));
    const int displayInnings = std::max(9, maxInnings);

    // Line score header
    out << "\nLine Score\n";
    out << std::left << std::setw(18) << " ";
    for (int i = 1; i <= displayInnings; ++i) {
        out << std::right << std::setw(3) << i;
    }
    out << "  |" << std::setw(4) << "R" << std::setw(4) << "H" << std::setw(4) << "E" << "\n";

    // Away row
    out << std::left << std::setw(18) << awayTeam_.name();
    for (int i = 0; i < displayInnings; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (idx < awayLineScore_.size() && awayLineScore_[idx] >= 0) {
            out << std::right << std::setw(3) << awayLineScore_[idx];
        } else {
            out << std::right << std::setw(3) << "x";
        }
    }
    out << "  |" << std::right << std::setw(4) << awayBoxScore_.runs
        << std::setw(4) << awayBoxScore_.hits
        << std::setw(4) << awayBoxScore_.errors << "\n";

    // Home row
    out << std::left << std::setw(18) << homeTeam_.name();
    for (int i = 0; i < displayInnings; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (idx < homeLineScore_.size() && homeLineScore_[idx] >= 0) {
            out << std::right << std::setw(3) << homeLineScore_[idx];
        } else {
            out << std::right << std::setw(3) << "x";
        }
    }
    out << "  |" << std::right << std::setw(4) << homeBoxScore_.runs
        << std::setw(4) << homeBoxScore_.hits
        << std::setw(4) << homeBoxScore_.errors << "\n";

    // Player stats
    auto printPlayerStats = [&out](const std::vector<PlayerBoxScore>& players,
                                   const std::string& teamName) {
        out << "\n" << teamName << "\n";
        out << std::left << std::setw(18) << "Player"
            << std::right << std::setw(4) << "AB" << std::setw(4) << "H"
            << std::setw(4) << "2B" << std::setw(4) << "3B" << std::setw(4) << "HR"
            << std::setw(4) << "BB" << std::setw(4) << "K"
            << std::setw(4) << "TB" << std::setw(4) << "RBI"
            << std::setw(7) << "AVG" << std::setw(7) << "OBP" << std::setw(7) << "SLG" << "\n";

        for (const auto& p : players) {
            const int pa = p.atBats + p.walks + p.hitByPitch + p.sacFlies;
            const double avg = p.atBats > 0 ? static_cast<double>(p.hits) / p.atBats : 0.0;
            const double obp = pa > 0 ? static_cast<double>(p.hits + p.walks + p.hitByPitch) / pa : 0.0;
            const double slg = p.atBats > 0 ? static_cast<double>(p.totalBases) / p.atBats : 0.0;

            out << std::left << std::setw(18) << p.name
                << std::right << std::setw(4) << p.atBats << std::setw(4) << p.hits
                << std::setw(4) << p.doubles << std::setw(4) << p.triples
                << std::setw(4) << p.homeRuns << std::setw(4) << p.walks
                << std::setw(4) << p.strikeouts << std::setw(4) << p.totalBases
                << std::setw(4) << p.rbi
                << std::fixed << std::setprecision(3)
                << std::setw(7) << avg << std::setw(7) << obp << std::setw(7) << slg << "\n";
        }
    };

    printPlayerStats(awayPlayerStats_, awayTeam_.name());
    printPlayerStats(homePlayerStats_, homeTeam_.name());
}

std::string GameEngine::inningLabel() const {
    return std::string(state_.isTop ? "Top " : "Bottom ") + ordinal(state_.inning);
}

std::string GameEngine::ordinal(int inning) {
    if (inning % 100 >= 11 && inning % 100 <= 13) {
        return std::to_string(inning) + "th";
    }

    switch (inning % 10) {
        case 1:
            return std::to_string(inning) + "st";
        case 2:
            return std::to_string(inning) + "nd";
        case 3:
            return std::to_string(inning) + "rd";
        default:
            return std::to_string(inning) + "th";
    }
}

} // namespace joji
