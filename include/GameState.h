#pragma once

#include <array>
#include <optional>
#include <string>

namespace joji {

struct GameState {
    int inning = 1;
    bool isTop = true;
    int outs = 0;
    std::array<std::optional<std::string>, 3> bases{};
    int awayScore = 0;
    int homeScore = 0;
    int awayPitcherPitchCount = 0; // away投手の通算球数
    int homePitcherPitchCount = 0; // home投手の通算球数
};

} // namespace joji
