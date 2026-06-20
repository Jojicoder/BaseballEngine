#pragma once

#include <string>

namespace joji {

class GameEngine;

// Serialize a completed game to a JSON string.
// Includes: teams, score, result, line score, per-player stats.
std::string exportGameToJson(const GameEngine& engine);

} // namespace joji
