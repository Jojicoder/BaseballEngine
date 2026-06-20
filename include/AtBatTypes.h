#pragma once

#include "BallPhysicsEngine.h"
#include "Player.h"

#include <optional>
#include <string>
#include <vector>

namespace joji {

struct Count {
    int balls = 0;
    int strikes = 0;
};

bool isWalk(const Count& count);
bool isStrikeOut(const Count& count);
Count afterBall(Count count);
Count afterStrike(Count count);
Count afterFoul(Count count);

enum class PitchType {
    Fastball,
    Slider,
    Curveball,
    Changeup,
    Cutter,
    Splitter
};

struct Pitch {
    PitchType pitchType = PitchType::Fastball;
    double pitchVelocity = 0.0;
    double locationX = 0.0;
    double locationZ = 0.0;
    double movementX = 0.0;
    double movementZ = 0.0;
    double spinRate = 0.0;
    double pitchQuality = 0.0;
};

enum class SwingDecisionType {
    Swing,
    Take
};

struct SwingDecision {
    SwingDecisionType decision = SwingDecisionType::Take;
    double swingProbability = 0.0;
    std::string reason;
};

enum class ZoneResultType {
    Ball,
    CalledStrike
};

struct ZoneResult {
    ZoneResultType result = ZoneResultType::Ball;
    double locationX = 0.0;
    double locationZ = 0.0;
};

struct Swing {
    double swingTiming = 0.0;
    double timingError = 0.0;
    double attackAngle = 0.0;
    double powerIntent = 0.0;
    double contactIntent = 0.0;
    double batSpeed = 0.0;
    double swingQuality = 0.0;
};

enum class ContactResultType {
    SwingMiss,
    Foul,
    InPlay
};

struct ContactResult {
    ContactResultType resultType = ContactResultType::SwingMiss;

    // These describe contact quality only. Final play outcomes are resolved later.
    double barrelAccuracy = 0.0;
    double timingQuality = 0.0;
    double verticalBatError = 0.0;
    double horizontalBatError = 0.0;
    double contactDepth = 0.0;
    double contactHeight = 0.0;
    bool isJammed = false;
    bool isTopped = false;
    bool isUnderBall = false;

    std::optional<BattedBallInput> battedBallInput;
};

enum class PitchOutcome {
    Ball,
    CalledStrike,
    SwingingStrike,
    Foul,
    InPlay
};

struct PitchLog {
    int pitchNumber = 0;
    Count countBefore;
    Pitch pitch;
    SwingDecision swingDecision;
    std::optional<ZoneResult> zoneResult;
    std::optional<Swing> swing;
    std::optional<ContactResult> contactResult;
    PitchOutcome pitchOutcome = PitchOutcome::Ball;
    Count countAfter;
};

// 投球間に発生した走塁イベント (盗塁・ボーク・ワイルドピッチ等)
enum class BaseRunningEventType {
    StolenBase,      // 盗塁成功
    CaughtStealing,  // 盗塁刺
    WildPitch,       // ワイルドピッチ
    PassedBall,      // パスボール
    Balk,            // ボーク
    PickoffAttempt,  // 牽制球 (走者セーフ)
    PickoffOut       // 牽制球アウト
};

struct BaseRunningEvent {
    BaseRunningEventType type  = BaseRunningEventType::StolenBase;
    std::string runnerName;   // 関係した走者 (WP/PB/Balk は省略可)
    int fromBase  = 0;        // 出発塁 (1/2/3)
    int toBase    = 0;        // 到達塁 (2/3/4=home)
    bool scored   = false;    // 得点したか
    int afterPitch = 0;       // 何球目の後 (0 = 1球目の前)
};

enum class AtBatOutcome {
    Walk,
    StrikeOut,
    InPlay,
    HitByPitch
};

struct AtBatResult {
    Player batter;
    Player pitcher;
    AtBatOutcome finalOutcome = AtBatOutcome::StrikeOut;
    Count finalCount;
    std::vector<PitchLog> pitchLogs;
    std::vector<BaseRunningEvent> baseRunningEvents;
    std::optional<BattedBallInput> battedBallInput;
};

// 1球ずつ進むモード用: 打席の進行中状態
struct AtBatState {
    Player batter;
    Player pitcher;
    Count count;
    int pitchNumber = 1;
    std::vector<PitchLog> pitchLogs;
    std::vector<BaseRunningEvent> baseRunningEvents; // 打席中の走塁イベント
    bool isFinished = false;
    std::optional<AtBatOutcome> finalOutcome;
    std::optional<BattedBallInput> battedBallInput;
    bool buntIntent = false; // バント意図: GameEngine が状況に応じて設定
};

std::string toString(PitchType type);
std::string toString(SwingDecisionType type);
std::string toString(ZoneResultType type);
std::string toString(ContactResultType type);
std::string toString(PitchOutcome outcome);
std::string toString(AtBatOutcome outcome);

} // namespace joji
