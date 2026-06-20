#pragma once

#include "Player.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace joji {

class Team {
public:
    // lineup = 打順8人 (投手含まず), starter = 先発投手, bullpen = 中継ぎ/抑え
    Team(std::string teamName, std::vector<Player> lineup,
         Player starter, std::vector<Player> bullpen);

    const std::string& name() const;
    const std::vector<Player>& lineup() const;   // 打者のみ (8人)
    const Player& currentBatter() const;
    int currentBattingIndex() const;
    void advanceBatter();
    void retreatBatter();  // CS等で打席未完了のままイニング終了した場合に打順を戻す

    const Player& starter() const;
    const std::vector<Player>& bullpen() const;

private:
    std::string name_;
    std::vector<Player> lineup_;        // 打者8人
    Player starter_;
    std::vector<Player> bullpen_;       // LongRelief/Middle×2/Setup/Closer
    int currentBattingIndex_ = 0;
};

} // namespace joji
