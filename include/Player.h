#pragma once

#include <string>
#include <vector>

namespace joji {

// Moved here from AtBatTypes.h so Player can reference it without circular includes.
enum class PitchType {
    Fastball,
    Slider,
    Curveball,
    Changeup,
    Cutter,
    Splitter
};

struct PitchGrade {
    PitchType type;
    int grade = 50; // scout scale 40–80
};

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

// Player role — primary function for roster construction and trade valuation.
enum class PlayerRole {
    Leadoff,        // contact/eye/speed 特化
    ContactHitter,  // 安打製造機
    PowerHitter,    // クリーンアップ power 特化
    CornerIF,       // 1B/3B  power+fielding
    MiddleIF,       // 2B/SS  fielding/arm+speed
    CenterFielder,  // speed+fielding 特化
    CornerOF,       // LF/RF  power or speed
    Catcher,        // arm+fielding 特化
    UtilityIF,      // 複数ポジション、代走/守備交代
    ExtraOF,        // 4th OF
    PinchHitter,    // contact/eye 高め、fielding 低め
    BackupCatcher,
    Ace,
    Starter,        // #2-#3
    BackOfRotation, // #4-#5
    Closer,
    Setup,
    MiddleRelief,
    LongRelief,
    Specialist,     // 対左/右専門
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

    // Pitcher arsenal: which pitch types and how good each one is.
    std::vector<PitchGrade> arsenal;

    // Batter tendencies
    int pullTendency       = 50; // >50 = pull hitter, <50 = opposite field
    int highBallHitter     = 50; // >50 = strong on high pitches
    int chaseRate          = 50; // >50 = chases off-zone pitches more
    int contactVsBreaking  = 50; // >50 = handles breaking balls well
    int clutchRating       = 50; // >50 = performs better in high-leverage situations

    // Roster role — set after construction via withRole(); used for trade valuation
    PlayerRole role = PlayerRole::ContactHitter;
};

} // namespace joji
