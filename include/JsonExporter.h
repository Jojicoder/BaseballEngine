#pragma once

#include <string>

namespace joji {

struct AnimationPlan;
class GameEngine;

// Serialize a completed game to a JSON string.
// Includes: teams, score, result, line score, per-player stats.
std::string exportGameToJson(const GameEngine& engine);

// Serialize one play animation/replay plan.
// Includes the unified ReplayTimeline used by renderers and external viewers.
std::string exportAnimationPlanToJson(const AnimationPlan& plan);

} // namespace joji
