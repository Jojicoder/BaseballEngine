#pragma once

#include "Player.h"
#include "PlayResolutionEngine.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace joji {

class Team {
public:
    // lineup = 打順8人, rotation = 先発5人 (ローテーション順), bullpen = 中継ぎ/抑え
    Team(std::string teamName, std::vector<Player> lineup,
         std::vector<Player> rotation, std::vector<Player> bullpen,
         std::vector<Player> bench = {},
         BallparkConfig ballpark = BallparkConfig{});

    const std::string& name() const;
    const std::vector<Player>& lineup() const;   // 打者のみ (8人)
    const Player& currentBatter() const;
    const Player& nextBatter() const;            // 次の打者 (IBB 用)
    int currentBattingIndex() const;
    void advanceBatter();
    void retreatBatter();

    const Player& starter() const;              // 今試合の先発 (rotationSlot_ 番目)
    const std::vector<Player>& rotation() const;
    void setStarterSlot(int slot);              // season_runner がゲームごとに呼ぶ
    const std::vector<Player>& bullpen() const;
    const BallparkConfig& homeBallpark() const;

    // ベンチ / 代打
    const std::vector<Player>& bench() const;
    const std::vector<bool>& usedBench() const;
    bool hasBenchPlayer() const;
    void sendPinchHitter(std::size_t benchIndex, std::size_t lineupIndex);

private:
    std::string name_;
    std::vector<Player> lineup_;
    std::vector<Player> rotation_;      // 先発5人 (index 0 = エース)
    int rotationSlot_ = 0;
    std::vector<Player> bullpen_;
    int currentBattingIndex_ = 0;

    std::vector<Player> bench_;
    std::vector<bool> usedBench_;
    BallparkConfig homeBallpark_;
};

} // namespace joji
