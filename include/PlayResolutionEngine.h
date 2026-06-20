#pragma once

#include "GameState.h"
#include "PlayResult.h"
#include "Random.h"

#include <array>
#include <string>

namespace joji {

enum class PlayOutcomeType {
    Foul,
    Out,
    Single,
    Double,
    Triple,
    HomeRun
};


enum class FieldPosition {
    Pitcher,
    Catcher,
    FirstBase,
    SecondBase,
    ThirdBase,
    Shortstop,
    LeftField,
    CenterField,
    RightField
};

struct Fielder {
    int id = -1;
    std::string name;
    FieldPosition position = FieldPosition::Pitcher;
    Vector3 startPosition;
    double speedFeetPerSecond = 16.0;
    double reactionSeconds = 0.3;
    double fielding = 0.9;
    double armStrength = 40.0;
    double routeEfficiency = 0.9;
};

struct DefenseAlignment {
    std::array<Fielder, 9> fielders{};

    static DefenseAlignment standard();
};

struct BallparkConfig {
    std::string name             = "Generic Park";
    double fenceHeightFeet       = 7.0;
    double centerFieldFenceFeet  = 400.0;  // CF フェンス距離 (ft)
    double gapFenceFeet          = 365.0;  // ギャップ (LCF/RCF) フェンス距離
    double cornerFenceFeet       = 325.0;  // コーナー (LF/RF) フェンス距離

    static BallparkConfig generic()      { return {}; }
    static BallparkConfig hittersPark()  { return {"Hitter's Park",  7.0, 385.0, 350.0, 308.0}; }
    static BallparkConfig pitchersPark() { return {"Pitcher's Park", 7.0, 418.0, 385.0, 345.0}; }
};

struct PlayResolution {
    PlayOutcomeType     type           = PlayOutcomeType::Out;
    FieldingOutcomeType fieldingOutcome = FieldingOutcomeType::NotApplicable;
    int outsRecorded = 0;
    int basesAwarded = 0;
    int fielderId = -1;
    std::string fielderName;
    bool madeFieldingPlay = false;
    Vector3 fieldingPoint;
    double fielderTravelTime = 0.0;
    double fieldingAvailableTime = 0.0;
    std::string description;
    bool infieldFly = false;
};

class PlayResolutionEngine {
public:
    PlayResolution resolve(const BattedBall& ball,
                           const GameState& state,
                           Random& random,
                           const DefenseAlignment& defense = DefenseAlignment{},
                           const BallparkConfig& ballpark = BallparkConfig{}) const;

private:
    static bool isGroundBall(const BattedBall& ball);
};

std::string toString(PlayOutcomeType type);

} // namespace joji
