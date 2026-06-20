#pragma once

#include "Team.h"

#include <vector>

namespace joji {

// 全登録チームを返す。インデックス順がデフォルトのリーグ順。
std::vector<Team> allTeams();

} // namespace joji
