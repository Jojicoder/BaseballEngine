#pragma once

#include "GameState.h"
#include "Player.h"
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
    double speedFeetPerSecond = 26.0;
    double reactionSeconds = 0.3;
    double fielding = 0.9;
    double armStrength = 40.0;
    double routeEfficiency = 0.9;
    double accelerationFeetPerSecond2 = 13.0; // ramp-up; typical fielder ≈13 ft/s²
};

struct DefenseAlignment {
    std::array<Fielder, 9> fielders{};

    static DefenseAlignment standard();
};

struct BallparkConfig {
    std::string name             = "Generic Park";
    double fenceHeightFeet       = 7.0;
    double centerFieldFenceFeet  = 400.0;
    double gapFenceFeet          = 365.0;
    double cornerFenceFeet       = 325.0;
    double altitudeFeet          = 0.0;    // elevation above sea level (ft)
    double temperatureFahrenheit = 72.0;   // game-time temperature
    // Wind: direction 0°=blowing toward CF (tailwind), 90°=L→R, 180°=blowing in (headwind)
    double windSpeedMph          = 0.0;
    double windDirectionDeg      = 0.0;

    static BallparkConfig generic()      { return {}; }
    static BallparkConfig hittersPark()  { return {"Hitter's Park",  7.0, 385.0, 350.0, 308.0}; }
    static BallparkConfig pitchersPark() { return {"Pitcher's Park", 7.0, 418.0, 385.0, 345.0}; }
    static BallparkConfig highAltitude() { return {"High Altitude Park", 8.0, 415.0, 390.0, 347.0, 5200.0, 68.0}; }
    // Wind presets (sea-level, average temp)
    static BallparkConfig windOut(double mph = 10.0) {
        BallparkConfig c; c.windSpeedMph = mph; c.windDirectionDeg = 0.0; return c;
    }
    static BallparkConfig windIn(double mph = 10.0) {
        BallparkConfig c; c.windSpeedMph = mph; c.windDirectionDeg = 180.0; return c;
    }

    // ── Team home ballparks ───────────────────────────────────────────────────
    // Fence distances are the primary park-factor lever. Wind is avoided because
    // even 5-8 mph has outsized effects on both teams equally.

    // Newark Knights — Knights Field
    // Compact park. Short corners suit the speed/gap-hitting style.
    static BallparkConfig knightsField() {
        return {"Knights Field", 7.0, 400.0, 358.0, 312.0, 0.0, 71.0};
    }
    // Queens Titans — Queens Coliseum
    // Spacious modern park. Deep gaps; slight pitcher lean.
    static BallparkConfig queensColiseum() {
        return {"Queens Coliseum", 8.0, 412.0, 375.0, 332.0, 0.0, 69.0};
    }
    // Brooklyn Hammers — Iron Yard
    // Hitter's park. Short corners and warm urban air boost HR.
    static BallparkConfig ironYard() {
        return {"Iron Yard", 7.0, 396.0, 354.0, 308.0, 0.0, 76.0};
    }
    // Bronx Wolves — Wolf Den
    // Classic dimensions, neutral-to-hitter-friendly corners.
    static BallparkConfig wolfDen() {
        return {"Wolf Den", 7.0, 406.0, 363.0, 316.0, 0.0, 72.0};
    }
    // Harlem Eagles — Eagle Park
    // True neutral park. No park factor advantage or penalty.
    static BallparkConfig eaglePark() {
        return {"Eagle Park", 7.0, 404.0, 368.0, 325.0, 0.0, 71.0};
    }
    // Staten Island Foxes — Fox Field
    // Mildly large park — slight pitcher-friendly edge without extreme suppression.
    static BallparkConfig foxField() {
        return {"Fox Field", 7.0, 408.0, 372.0, 330.0, 0.0, 70.0};
    }

    // ── League B — Philadelphia district ballparks ────────────────────────────
    // Fishtown Ferals — Ferals Field
    // Short corners, warm urban air. Hitter's park in a tight neighborhood.
    static BallparkConfig feralsField() {
        return {"Ferals Field", 7.0, 395.0, 352.0, 308.0, 0.0, 75.0};
    }
    // Kensington Iron — Iron Works
    // Industrial neighborhood. Slightly hitter-friendly corners.
    static BallparkConfig ironWorks() {
        return {"Iron Works", 7.0, 402.0, 360.0, 316.0, 0.0, 72.0};
    }
    // Germantown Colonials — Colonial Field
    // Historic, symmetrical. True neutral park.
    static BallparkConfig colonialField() {
        return {"Colonial Field", 7.0, 404.0, 368.0, 324.0, 0.0, 71.0};
    }
    // Manayunk Runners — Canal Park
    // Built along the Schuylkill canal. Slightly spacious, suits contact play.
    static BallparkConfig canalPark() {
        return {"Canal Park", 7.0, 406.0, 370.0, 328.0, 0.0, 70.0};
    }
    // Fairmount Rams — Fairmount Field
    // Large park inspired by Fairmount Park. Pitcher-friendly gaps.
    static BallparkConfig fairmountField() {
        return {"Fairmount Field", 7.0, 408.0, 374.0, 330.0, 0.0, 70.0};
    }
    // South Philly Stallions — Stallions Field
    // Biggest park in the league. Deep fences suit a pitching-first rebuild.
    static BallparkConfig stallionsField() {
        return {"Stallions Field", 7.0, 410.0, 376.0, 332.0, 0.0, 69.0};
    }
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

DefenseAlignment applyDefensiveShift(DefenseAlignment alignment, const Player& batter);

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
