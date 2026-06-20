#pragma once

#include <string>

namespace joji {

enum class PitcherRole {
    Starter,
    LongRelief,
    MiddleRelief,
    Setup,
    Closer
};

std::string toString(PitcherRole role);

enum class ThrowingHand {
    Right,
    Left
};

enum class BattingSide {
    Right,
    Left,
    Switch
};

struct MatchupContext {
    ThrowingHand pitcherHand = ThrowingHand::Right;
    BattingSide batterSide = BattingSide::Right;
    bool sameHanded = true;
    bool oppositeHanded = false;
    bool platoonAdvantage = false;
};

enum class Position {
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

struct Player {
    std::string name;
    Position    position        = Position::CenterField;
    int contact         = 50;
    int power           = 50;
    int eye             = 50;
    int speed           = 50;
    int fielding        = 50;
    int arm             = 50;
    int pitchingVelocity = 50;
    int pitchingControl  = 50;
    int pitchingStuff    = 50;
    int pitchingStamina  = 50; // 高いほど球数増加時の劣化が緩やか
    PitcherRole pitcherRole = PitcherRole::Starter;
    ThrowingHand throwingHand = ThrowingHand::Right;
    BattingSide battingSide = BattingSide::Right;
};

} // namespace joji
