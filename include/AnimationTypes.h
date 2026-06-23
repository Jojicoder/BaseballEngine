#pragma once

#include "Player.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace joji {

struct AnimationPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double timeSeconds = 0.0;
};

struct PitchAnimation {
    std::string pitcherName;
    std::string batterName;
    std::string pitchType;
    AnimationPoint start;
    AnimationPoint end;
    std::vector<AnimationPoint> points;
    double durationSeconds = 0.0;
    double velocity = 0.0;
    double movementX = 0.0;
    double movementZ = 0.0;
    double endLocationX = 0.0;
    double endLocationZ = 0.0;
    int zoneNumber = 0;
    ThrowingHand pitcherHand = ThrowingHand::Right;
    BattingSide batterSide = BattingSide::Right;
    std::string result;
    bool isStrike = false;
    bool isBall = false;
    bool isInPlay = false;
};

struct BattedBallAnimation {
    std::string batterName;
    std::string result;
    std::string classification;
    std::vector<AnimationPoint> points;
    AnimationPoint landingPoint;
    AnimationPoint finalPoint;
    double durationSeconds = 0.0;
    double estimatedDistance = 0.0;
    double maxHeight = 0.0;
    double fenceCrossSeconds = -1.0; // crossesFence=true の場合のフェンス通過時刻
    bool landsFair = false;
    bool crossesFence = false;
};

struct RunnerAnimation {
    std::string runnerName;
    int fromBase = 0; // 0 = home, 1 = first, 2 = second, 3 = third
    int toBase = 0;   // 4 = scored
    bool scored = false;
    bool out = false;
    bool usedForDisplay = true;
    std::string outAtBase;
    std::vector<AnimationPoint> points;
    double durationSeconds = 0.0;
};

struct DefenseAnimation {
    int    fielderId       = -1;
    std::string fielderName;
    bool   madePlay        = false;
    double durationSeconds = 0.0;
    std::vector<AnimationPoint> points; // field xy path (z unused)
};

struct ThrowAnimation {
    int    fielderId        = -1;
    std::string fielderName;
    int    targetBase       = 0;  // 1=1B 2=2B 3=3B 4=home
    double startTimeOffset  = 0.0; // relative to field-view t=0
    double durationSeconds  = 0.0; // ball flight time
    bool badThrow = false;
    double offlineFeet = 0.0;
    double arcHeightFeet = 0.0; // parabolic arc peak height; 0 = flat (very short throws)
    std::vector<AnimationPoint> points; // [start, end] in field xy
};

struct RunnerMovement {
    std::string runnerName;
    int fromBase = 0;
    int toBase = 0;
    double startTime = 0.0;
    double arrivalTime = 0.0;
    double topSpeedFeetPerSecond = 0.0;
    double accelerationFeetPerSecond2 = 0.0;
    double turnPenaltySeconds = 0.0;
    double slideTimeSeconds = 0.0;
    bool usedForSafeOut = true;
    bool scored = false;
    bool out = false;
    std::vector<AnimationPoint> routePoints;
};

struct ThrowMovement {
    std::string fielderName;
    AnimationPoint fromPosition;
    int targetBase = 0;
    double startTime = 0.0;
    double arrivalTime = 0.0;
    double velocityFeetPerSecond = 0.0;
    double accuracy = 1.0;
    std::vector<AnimationPoint> trajectory;
};

struct TagPlay {
    int base = 0;
    std::string runnerName;
    double runnerArrivalTime = 0.0;
    double ballArrivalTime = 0.0;
    double tagTime = 0.0;
    bool runnerSafe = true;
};

struct ReplayDecision {
    int chosenTargetBase = 0;
    bool holdBall = true;
    bool cutoff = false;
    std::string reason;
};

enum class ReplayEventType {
    Pitch,
    Contact,
    BallFlight,
    Field,
    Throw,
    Runner,
    Tag,
    Result
};

enum class ReplayPhase {
    Pitch,
    Contact,
    BallFlight,
    Field,
    Throw,
    Runner,
    Tag,
    Result
};

struct ReplayEvent {
    ReplayEventType type = ReplayEventType::Result;
    double timeSeconds = 0.0;
    std::string label;
    std::string actor;
    std::string detail;
    std::string result;
    int base = 0;
    AnimationPoint position;
};

struct ReplayTimeline {
    std::string schemaName = "joji.replay.timeline";
    int schemaVersion = 1;
    std::vector<ReplayEvent> events;
    double durationSeconds = 0.0;
};

struct ReplayCursor {
    double elapsedSeconds = 0.0;
    ReplayPhase phase = ReplayPhase::Result;
    std::optional<std::size_t> activeEventIndex;
    std::optional<std::size_t> nextEventIndex;
    std::vector<std::size_t> previousEventIndices;
};

inline ReplayPhase phaseForReplayEvent(ReplayEventType type) {
    switch (type) {
        case ReplayEventType::Pitch:      return ReplayPhase::Pitch;
        case ReplayEventType::Contact:    return ReplayPhase::Contact;
        case ReplayEventType::BallFlight: return ReplayPhase::BallFlight;
        case ReplayEventType::Field:      return ReplayPhase::Field;
        case ReplayEventType::Throw:      return ReplayPhase::Throw;
        case ReplayEventType::Runner:     return ReplayPhase::Runner;
        case ReplayEventType::Tag:        return ReplayPhase::Tag;
        case ReplayEventType::Result:     return ReplayPhase::Result;
    }
    return ReplayPhase::Result;
}

inline ReplayCursor cursorForReplayTimeline(const ReplayTimeline& timeline,
                                            double elapsedSeconds) {
    ReplayCursor cursor;
    cursor.elapsedSeconds = elapsedSeconds;
    for (std::size_t i = 0; i < timeline.events.size(); ++i) {
        if (timeline.events[i].timeSeconds <= elapsedSeconds + 0.001) {
            if (!cursor.activeEventIndex.has_value()) {
                cursor.activeEventIndex = i;
                cursor.phase = phaseForReplayEvent(timeline.events[i].type);
            } else if (timeline.events[i].timeSeconds
                       > timeline.events[*cursor.activeEventIndex].timeSeconds + 0.001) {
                cursor.previousEventIndices.push_back(*cursor.activeEventIndex);
                cursor.activeEventIndex = i;
                cursor.phase = phaseForReplayEvent(timeline.events[i].type);
            } else {
                cursor.previousEventIndices.push_back(i);
                if (elapsedSeconds > timeline.events[i].timeSeconds + 0.001) {
                    cursor.activeEventIndex = i;
                    cursor.phase = phaseForReplayEvent(timeline.events[i].type);
                }
            }
        } else {
            cursor.nextEventIndex = i;
            break;
        }
    }
    if (!cursor.activeEventIndex.has_value() && !timeline.events.empty()) {
        cursor.nextEventIndex = 0;
        cursor.phase = phaseForReplayEvent(timeline.events.front().type);
    }
    return cursor;
}

struct AnimationPlan {
    std::string gameId;
    std::string description;
    PitchAnimation pitch;
    BattedBallAnimation battedBall;
    std::vector<RunnerAnimation> runners;
    std::vector<DefenseAnimation> defenders;
    std::vector<ThrowAnimation>   throws;
    std::vector<RunnerMovement> runnerMovements;
    std::vector<ThrowMovement> throwMovements;
    std::vector<TagPlay> tagPlays;
    ReplayDecision defensiveDecision;
    ReplayTimeline replayTimeline;
    double totalDurationSeconds = 0.0;
    bool hasPitch = false;
    bool hasBattedBall = false;
};

} // namespace joji
