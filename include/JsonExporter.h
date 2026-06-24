#pragma once

#include <map>
#include <string>
#include <vector>

namespace joji {

struct AnimationPlan;
class GameEngine;

// Per-team season summary passed to exportSeasonSummaryToJson.
struct TeamSeasonSummary {
    std::string name;
    std::string league;   // "A" or "B"
    int wins       = 0;
    int losses     = 0;
    double rsPerG  = 0.0;
    double raPerG  = 0.0;
    double teamEra = 0.0;
    double teamBa  = 0.0;
    double teamObp = 0.0;
    double teamSlg = 0.0;
    double teamWoba= 0.0;
    double kPct    = 0.0;
    double bbPct   = 0.0;
};

// Serialize a completed game to a JSON string.
// Includes: teams, score, result, line score, per-player batting + pitcher stats.
std::string exportGameToJson(const GameEngine& engine);

// Serialize one play animation/replay plan.
std::string exportAnimationPlanToJson(const AnimationPlan& plan);

// Serialize a season summary (standings + team aggregate stats) to JSON.
std::string exportSeasonSummaryToJson(const std::vector<TeamSeasonSummary>& teams,
                                      const std::string& worldSeriesChampion,
                                      int seasonNumber);

} // namespace joji
