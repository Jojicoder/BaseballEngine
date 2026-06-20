#pragma once

#include <string>
#include <vector>

namespace joji {

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class AtBatResultType {
    StrikeOut,
    Walk,
    GroundOut,
    FlyOut,
    Single,
    Double,
    Triple,
    HomeRun,
    Error,      // 守備エラーで出塁 (公式 AB にはカウント、ヒットにはならない)
    FielderChoice, // フィルダースチョイス: 守備が先の走者を狙い打者が出塁
    InfieldFly, // インフィールドフライ: 打者自動アウト
    SacrificeBunt,      // 犠打: 打者アウト、走者進塁、AB なし
    SqueezeBunt,        // スクイズ: 3B走者生還、打者アウト、AB なし
    BuntSingle,         // バント安打: 打者1B出塁
    BuntFielderChoice   // バントFC: 守備が先の走者を狙い打者1B出塁
};

// 守備側がどう処理したか (PlayOutcomeType とは独立)
enum class FieldingOutcomeType {
    NotApplicable,  // Walk / K / HR / Foul
    Caught,         // フライ捕球
    FieldedCleanly, // ゴロ処理
    Error,          // エラー発生
    NotFielded,     // 誰も追いつかなかった (ヒット確定)
};

struct BattedBall {
    double exitVelocity = 0.0;
    double launchAngle = 0.0;
    double sprayAngle = 0.0;
    double sideSpin = 0.0;
    double backSpin = 0.0;
    double estimatedDistance = 0.0;
    double hangTime = 0.0;
    double maxHeight = 0.0;
    Vector3 landingPoint;
    Vector3 landingVelocity;
    std::vector<Vector3> bounceTrajectory;
    Vector3 finalRestPoint;
    double groundRollDistance = 0.0;
    bool landsFair = false;
    bool crossesFence = false;
    bool isBunt = false;
    double fenceDistance = 0.0;
    double fenceCrossHeight = 0.0;
    Vector3 fenceIntersectionPoint;
    std::string classification;
    std::vector<Vector3> trajectory;
};

// What throw(s) the fielder made — set by applyPlay(), used by ThrowAnimation
struct ThrowDecision {
    int  targetBase = 0;    // 0=no throw, 1=1B, 2=2B, 3=3B, 4=home
    bool doublePlay = false; // true → first throw to 2B, then 2B→1B
    std::vector<int> targetSequence; // ordered force/out bases, e.g. {3,2,1}
};

struct RunnerOut {
    std::string runnerName;
    int fromBase = 0; // 0 = batter/home, 1/2/3 = occupied base before play
    int outAtBase = 0; // 1/2/3/4(home)
    bool forceOut = false;
};

struct PlayResult {
    AtBatResultType     type           = AtBatResultType::StrikeOut;
    FieldingOutcomeType fieldingOutcome = FieldingOutcomeType::NotApplicable;
    std::string batterName;
    std::string pitcherName;
    int fielderId = -1;
    std::string fielderName;
    bool madeFieldingPlay = false;
    Vector3 fieldingPoint;
    double fielderTravelTime = 0.0;
    double fieldingAvailableTime = 0.0;
    int runsScored = 0;
    int outsRecorded = 0;
    BattedBall battedBall;
    ThrowDecision throwDecision;
    std::vector<RunnerOut> runnerOuts;
    std::vector<std::string> events;
    bool infieldFly = false;
};

struct GameLog {
    int inning = 1;
    bool isTop = true;
    std::string text;
};

std::string toString(AtBatResultType type);

enum class GameResultType {
    AwayWin,
    HomeWin,
    Tie
};

struct GameResult {
    GameResultType type = GameResultType::Tie;
    int awayScore = 0;
    int homeScore = 0;
    int innings = 9;
};

} // namespace joji
