#pragma once

#include "Player.h"
#include "Random.h"
#include "Team.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace joji {

// ── Draft engine (v1: fully random) ───────────────────────────────────────────
//
// See docs/draft-engine-design.md.  Three independent steps so each can be
// tested on its own:
//
//   1. generateProspectPool()   – random draft-eligible rookies
//   2. runDraft()               – every team picks `rounds` times (snake order)
//   3. applyDraftToRosters()    – rookies who clearly beat a bench/bullpen
//                                 player take that spot; the rest go to the minors
//
// Everything is driven by DraftConfig::seed, so the same inputs always produce
// the same draft.  Ratings use the engine's own scale (PLAYER_ARCHETYPES.md),
// not the 20-80 scouting scale.

struct DraftConfig {
    int season = 2027;
    unsigned seed = 1;
    int rounds = 7;
    double poolMultiplier = 1.5;  // pool size = teams * rounds * poolMultiplier
    int promotionMargin = 5;      // rookie must beat the replaced player's overall by this much (~1.8 moves per team)
};

struct DraftProspect {
    int id = 0;
    std::string name;
    int age = 20;
    Position position = Position::CenterField;
    PlayerRole role = PlayerRole::ContactHitter;
    int potential = 50;  // ceiling tier (not used by v1 ratings; kept for the future growth phase)
    int readiness = 50;  // current tier: 62 = a typical regular for the role
    int overall = 0;     // composite of asPlayer's ratings (see playerOverall)
    Player asPlayer;
};

struct DraftPick {
    int round = 0;
    int pickInRound = 0;
    int overall = 0;  // 1-based across the whole draft
    int teamIdx = 0;
    std::string team;
    int prospectId = 0;
};

enum class RosterChangeType {
    Promoted,  // rookie took a bench/bullpen spot
    Released,  // the player who lost that spot
    Minors,    // drafted but did not make the roster
};

struct RosterChange {
    std::string team;
    RosterChangeType type = RosterChangeType::Minors;
    std::string player;
    int prospectId = -1;  // the rookie involved (Promoted / Minors), -1 for Released
    std::string slot;     // "bench" | "bullpen" | "" (minors)
    std::string replaced; // Promoted only: who the rookie replaced
    int overall = 0;
};

struct DraftResult {
    int season = 0;
    unsigned seed = 0;
    int rounds = 0;
    std::vector<DraftProspect> pool;
    std::vector<DraftPick> picks;
    std::vector<RosterChange> changes;  // filled by applyDraftToRosters()
};

// Picks one prospect for `team` from `available` and returns its index in `available`.
using DraftStrategy = std::function<std::size_t(const std::vector<DraftProspect>& available,
                                                const Team& team,
                                                Random& rng)>;

DraftStrategy randomStrategy();

// Role-aware composite rating on the engine's scale (roughly 40-90).
int playerOverall(const Player& player);
// Same, but a pitcher is weighed as if holding `role` (used to compare a rookie to a bullpen slot).
int pitcherOverallAs(const Player& player, PitcherRole role);

std::vector<DraftProspect> generateProspectPool(int count,
                                                Random& rng,
                                                const std::vector<std::string>& takenNames);

DraftResult runDraft(const std::vector<Team>& teams,
                     const DraftConfig& config,
                     const DraftStrategy& strategy = randomStrategy());

// Returns the teams with promoted rookies swapped in.  Roster sizes never change.
// Fills result.changes.
std::vector<Team> applyDraftToRosters(const std::vector<Team>& teams,
                                      DraftResult& result,
                                      const DraftConfig& config);

} // namespace joji
