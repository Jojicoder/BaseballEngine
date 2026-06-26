#pragma once

#include "Player.h"

#include <string>

namespace joji {

struct PlayerBoxScore {
    std::string name;
    Position position = Position::CenterField;
    int atBats     = 0;
    int hits       = 0;
    int doubles    = 0;
    int triples    = 0;
    int homeRuns   = 0;
    int walks      = 0;
    int strikeouts = 0;
    int totalBases = 0;
    int rbi        = 0;
    int sacFlies   = 0;  // 犠牲フライ (AB にカウントしない)
    int sacBunts   = 0;  // 犠打 (AB にカウントしない)
    int gidp       = 0;  // 併殺打 (Ground Into Double Play)
    int reachedOnError = 0; // エラーで出塁
    int hitByPitch     = 0;  // 死球 (HBP)
    int stolenBases    = 0;  // 盗塁成功
    int caughtStealing = 0;  // 盗塁刺
    int putouts          = 0;
    int assists          = 0;
    int errors           = 0;
    int outfieldAssists  = 0;  // 本塁刺殺の外野補殺
    int rangeChances     = 0;  // candidate fielding plays with travel/available timing
    int rangePlays       = 0;  // range chances converted into a clean play/catch
};

struct PitcherBoxScore {
    std::string name;
    int games              = 0;
    int gamesStarted       = 0;
    int outsRecorded       = 0;  // 3 outs = 1 IP
    int runsAllowed        = 0;
    int earnedRuns         = 0;  // 現在は R = ER (非自責点なし)
    int strikeouts         = 0;
    int walks              = 0;
    int homeRunsAllowed    = 0;
    int hitsAllowed        = 0;
    int battersFaced       = 0;
    int wins               = 0;
    int saves              = 0;
    int holds              = 0;
    int blownSaves         = 0;
    int gamesFinished      = 0;
    int inheritedRunners   = 0;
    int inheritedRunnersScored = 0;

    double ip()    const { return outsRecorded / 3.0; }
    double era()   const { return outsRecorded > 0 ? earnedRuns            * 27.0 / outsRecorded : 0.0; }
    double whip()  const { return outsRecorded > 0 ? (walks + hitsAllowed) * 3.0  / outsRecorded : 0.0; }
    double kPer9() const { return outsRecorded > 0 ? strikeouts             * 27.0 / outsRecorded : 0.0; }
    double bbPer9()const { return outsRecorded > 0 ? walks                  * 27.0 / outsRecorded : 0.0; }
    double hr9()   const { return outsRecorded > 0 ? homeRunsAllowed        * 27.0 / outsRecorded : 0.0; }
};

// 試合開始時に全選手へ割り当てるコンディション (0.92〜1.08)
// battingForm  → contact / power / eye
// pitchingForm → velocity / control / stuff
// fieldingForm → fielding / arm / speed
struct PlayerGameForm {
    double battingForm  = 1.0;
    double pitchingForm = 1.0;
    double fieldingForm = 1.0;
};

} // namespace joji
