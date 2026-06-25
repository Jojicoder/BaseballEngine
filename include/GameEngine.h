#pragma once

#include "AnimationTypes.h"
#include "AtBatEngine.h"
#include "GameRules.h"
#include "GameState.h"
#include "PlayResolutionEngine.h"
#include "Stats.h"
#include "Team.h"

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace joji {

struct TeamBoxScore {
    int runs = 0;
    int hits = 0;
    int walks = 0;
    int atBats = 0;
    int strikeouts = 0;
    int homeRuns = 0;
    int totalBases = 0;
    int errors = 0;
    // v1.0 走塁指標
    int extraBasesTaken        = 0;  // XBT: 単打で1→3、単打で2→本、二塁打で1→本
    int runnerOutsOnBases      = 0;  // OOB: 走塁死 (合計)
    int runnersThrownOutAtHome = 0;  // TOH: 本塁刺殺
    int sacFlyAttempts         = 0;  // SFA: 犠牲フライ試み
    int sacFlySuccesses        = 0;  // SFS: 犠牲フライ成立
    int sacBunts               = 0;  // SAC: 犠打
    int doubles_               = 0;  // 2B hits (BABIP / 2B-BIP 計算用)
    int badThrows              = 0;
    int throwingErrors         = 0;
    int stolenBaseAttempts     = 0;
    int stolenBases            = 0;
    int caughtStealing         = 0;
    int wildPitches            = 0;
    int passedBalls            = 0;
    int balks                  = 0;
    int pickoffAttempts        = 0;
    int pickoffOuts            = 0;
    int buntAttempts           = 0;
    int buntSuccesses          = 0;
    int infieldFlies           = 0;
};

class GameEngine {
public:
    GameEngine(Team awayTeam, Team homeTeam, Random random,
               GameRules rules = GameRules{},
               BallparkConfig ballpark = BallparkConfig{},
               AtBatEngine atBatEngine = AtBatEngine{});

    void simulate(std::ostream& out);
    // 連投疲労マップ: pitcher name → 前回登板からの休養日数 (0=前日登板)
    void setFatigueMap(std::map<std::string, int> daysSinceLastPitch);
    // Hot/Cold ストリーク: player name → contact 補正値 (-5〜+5)
    void setStreakMap(std::map<std::string, double> streaks);
    // Fast mode: double physics timestep + skip trajectory recording (season sim only)
    void setFastMode(bool fast) { fastMode_ = fast; }

    std::optional<PlayResult> simulateNextPlay(std::ostream* out = nullptr);
    bool isComplete() const;
    GameResult result() const;
    const GameState& state() const;
    const std::vector<GameLog>& logs() const;
    const std::optional<AtBatResult>& lastAtBat() const;
    const std::optional<AnimationPlan>& latestAnimationPlan() const;
    const std::string& awayTeamName() const;
    const std::string& homeTeamName() const;
    const TeamBoxScore& awayBoxScore() const;
    const TeamBoxScore& homeBoxScore() const;
    // Line score: index i = inning (i+1), value -1 means inning was not played.
    const std::vector<int>& awayLineScore() const;
    const std::vector<int>& homeLineScore() const;
    // Player stats in batting order (indices 0–8).
    const std::vector<PlayerBoxScore>& awayPlayerStats() const;
    const std::vector<PlayerBoxScore>& homePlayerStats() const;
    const DefenseAlignment& currentDefenseAlignment() const;
    const BallparkConfig& ballpark() const;

    // Pitcher stats (one entry per pitcher who appeared)
    const std::vector<PitcherBoxScore>& awayPitcherStats() const;
    const std::vector<PitcherBoxScore>& homePitcherStats() const;

    // 1球ずつモード (SFML 用)
    bool simulateNextPitch();                            // 1球進める。true = 打席終了(pending)
    std::optional<PlayResult> applyPendingAtBatResult(); // pending を GameState に反映
    bool isAtBatInProgress() const;
    bool hasPendingAtBatResult() const;
    const AtBatState& currentAtBat() const;

    // 直近の simulateNextPitch() 呼び出しで発生した走塁イベント (SB/CS/WP/PB/Balk)
    const std::vector<BaseRunningEvent>& latestBaseRunningEvents() const;

    // 投手球数 (現在マウンドの投手)
    int currentPitcherPitchCount() const;
    // 現在マウンドの投手 (SFML 表示用)
    const Player& currentPitcher() const;

    // 打順 / ラインアップ (SFML 打順表示用)
    const std::vector<Player>& awayLineup() const;
    const std::vector<Player>& homeLineup() const;
    int awayBattingIndex() const;
    int homeBattingIndex() const;

    // 現在投手の球種別球数 (SFML アーセナル表示用)
    const std::map<std::string, int>& currentPitcherArsenal() const;

    // ─── SFML 表示用 ───────────────────────────────────────────────
    struct PitcherChangeEvent {
        std::string fromName;
        std::string toName;
        std::string reason;  // "SAVE SITUATION" / "FATIGUE" / "BLOWOUT" / "EARLY HOOK" / "PITCHING CHANGE"
    };
    const std::optional<PitcherChangeEvent>& lastPitcherChange() const;
    double pitcherFormValue() const;
    double batterFormValue(const std::string& playerName) const;
    double currentPitcherERA() const;
    // Returns estimated velocity drop (negative = slower) from fatigue, e.g. -3.5
    double currentPitcherVelocityDrop() const;

private:
    Team& battingTeam();
    Team& pitchingTeam();
    const Team& pitchingTeam() const;
    TeamBoxScore& battingBoxScore();
    TeamBoxScore& mutableAwayBoxScore();
    TeamBoxScore& mutableHomeBoxScore();
    std::vector<PlayerBoxScore>& battingPlayerStats();
    TeamBoxScore& pitchingBoxScore();
    std::vector<PlayerBoxScore>& pitchingPlayerStats();
    const DefenseAlignment& pitchingDefenseAlignment() const;

    bool shouldContinue() const;
    bool isBottomHalfOver() const;
    bool isWalkOffState() const;
    void initHalfInning();
    void commitHalfInning();
    void simulateHalfInning(std::ostream& out);
    PlayResult simulateAtBat(const Player& batter, const Player& pitcher);
    PlayResult buildPlayResult(const AtBatState& atBat);
    PlayResult applyPlay(const PlayResult& rawResult);
    Player effectivePitcher() const;
    Player formedBatter(const Player& batter, const Player& pitcher) const;
    MatchupContext matchupContext(const Player& batter, const Player& pitcher) const;
    void generateGameForms();
    void addPitchCount(int n);
    Player& currentPitcherMut();
    int& currentPitcherRunsAllowed();
    std::vector<bool>& currentBullpenUsed();
    void considerPitcherChange();
    void tryPitcherChange();
    void considerPinchHitter();
    void considerPinchRunner();
    void considerDefensiveReplacement();
    bool considerIntentionalWalk();
    void changePitcher(const Player& newPitcher, std::size_t bullpenIndex);
    void initPitcherSit();
    void finalizePitcherSit();
    void finalizeGameStats();
    void advanceRunnersForHit(PlayResult& result, int bases);
    void advanceRunnersForError(PlayResult& result);
    void advanceRunnersForWalk(PlayResult& result);
    void advanceAllRunnersOneBase(const std::string& reason);
    void checkPickoff();
    void checkStolenBase();
    void checkWildPitchPassedBall();
    void checkBalk();
    void scoreRunner(PlayResult& result, const std::string& runnerName);
    void recordPitchingPlay(const PlayResult& result);
    void recordDefensivePlay(const PlayResult& result);
    void clearBases();
    void nextHalfInning();
    void logPlay(const PlayResult& result, std::ostream* out);
    void printGameHeader(std::ostream& out) const;
    void printFinalScore(std::ostream& out) const;
    std::string inningLabel() const;
    static std::string ordinal(int inning);

    Team awayTeam_;
    Team homeTeam_;
    Random random_;
    GameRules rules_;
    BallparkConfig ballpark_;
    AtBatEngine atBatEngine_;
    BallPhysicsEngine ballPhysicsEngine_;
    PlayResolutionEngine playResolutionEngine_;
    GameState state_;

    // 投手管理 (awayDefense_ より先に宣言: コンストラクタで使用するため)
    Player currentAwayPitcher_;
    Player currentHomePitcher_;
    std::vector<bool> awayBullpenUsed_;
    std::vector<bool> homeBullpenUsed_;
    int awayCurrentPitcherRunsAllowed_ = 0;
    int homeCurrentPitcherRunsAllowed_ = 0;

    DefenseAlignment awayDefense_;
    DefenseAlignment homeDefense_;
    TeamBoxScore awayBoxScore_;
    TeamBoxScore homeBoxScore_;
    std::vector<int> awayLineScore_;
    std::vector<int> homeLineScore_;
    int currentHalfInningRuns_ = 0;
    std::vector<PlayerBoxScore> awayPlayerStats_;
    std::vector<PlayerBoxScore> homePlayerStats_;
    std::vector<GameLog> logs_;
    std::optional<AtBatResult> lastAtBat_;
    std::optional<AnimationPlan> latestAnimationPlan_;

    std::vector<PitcherBoxScore> awayPitcherStats_;
    std::vector<PitcherBoxScore> homePitcherStats_;

    // 試合ごとのコンディション (player.name → form)
    std::unordered_map<std::string, PlayerGameForm> gameforms_;

    // SV/HLD/BS/IR 追跡 (投手交代・失点のタイミングで更新)
    struct PitcherSitTracker {
        bool inSaveSit             = false;
        bool blownSave             = false;
        int  outsAtEntry           = 0;
        std::vector<std::string> inheritedRunners;
        int  inheritedRunnersScored = 0;
    };
    PitcherSitTracker awaySit_;
    PitcherSitTracker homeSit_;
    bool gameStatsFinalized_       = false;
    bool fastMode_                 = false;
    PitcherSitTracker& currentSit();

    std::optional<PitcherChangeEvent> lastPitcherChange_;

    // 現在投手の球種別球数 (投手交代でリセット)
    std::map<std::string, int> currentPitcherArsenal_;

    // 現在投手対各打者の打席数 (投手交代でリセット) — time-through-order 用
    std::unordered_map<std::string, int> currentPitcherABsVsBatter_;

    // 先発 W 資格: 5 回以上投げてリードで降板したとき true
    bool awayStarterPotentialWin_ = false;
    bool homeStarterPotentialWin_ = false;

    // 連投疲労マップ: pitcher name → 前回登板からの休養日数 (0=前日登板)
    std::map<std::string, int> fatigueMap_;

    // Hot/Cold ストリーク: player name → contact 補正値 (-5〜+5)
    std::map<std::string, double> streakMap_;

    // 試合内打者傾向メモリ: batter name → 前打席の chase/whiff 記録
    std::unordered_map<std::string, BatterHistory> batterHistory_;

    // 1球ずつモード用
    AtBatState currentAtBat_;
    bool atBatInProgress_ = false;
    std::optional<PlayResult> pendingAtBatResult_;
    std::vector<BaseRunningEvent> latestBaseRunningEvents_;
};

} // namespace joji
