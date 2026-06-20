#pragma once

#include "Player.h"

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
    bool landsFair = false;
    bool crossesFence = false;
};

struct RunnerAnimation {
    std::string runnerName;
    int fromBase = 0; // 0 = home, 1 = first, 2 = second, 3 = third
    int toBase = 0;   // 4 = scored
    bool scored = false;
    bool out = false;
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
    std::vector<AnimationPoint> points; // [start, end] in field xy
};

struct AnimationPlan {
    std::string gameId;
    std::string description;
    PitchAnimation pitch;
    BattedBallAnimation battedBall;
    std::vector<RunnerAnimation> runners;
    std::vector<DefenseAnimation> defenders;
    std::vector<ThrowAnimation>   throws;
    double totalDurationSeconds = 0.0;
    bool hasPitch = false;
    bool hasBattedBall = false;
};

} // namespace joji
