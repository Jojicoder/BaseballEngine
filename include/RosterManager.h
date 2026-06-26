#pragma once

#include "Roster.h"

#include <functional>
#include <random>
#include <string>
#include <vector>

namespace joji {

// ── RosterManager ─────────────────────────────────────────────────────────────
//
// Manages injuries, IL stints, minor-league call-ups, and player condition
// independently of the simulation engine.  It works by:
//   1. Reading game JSON output (via postGameUpdate()) after each simulated game
//   2. Rolling injury dice for players who appeared
//   3. Tracking IL durations and returning players when eligible
//   4. Managing condition (hot/cold streaks) based on recent performance
//
// This class is engine-agnostic: it can be used for any league that produces
// the same JSON game format.  No changes to GameEngine are needed.
//
class RosterManager {
public:
    explicit RosterManager(InjuryRiskConfig riskConfig = {});

    // ── Setup ─────────────────────────────────────────────────────────────────

    // Load a team's initial roster (called once per team at season start).
    void registerTeam(TeamRoster roster);

    // Remove all data for a league reset.
    void clear();

    // ── Post-game processing ──────────────────────────────────────────────────

    // Call after each simulated game with the date and names of all players
    // who appeared (batters who had an at-bat, pitchers who faced a batter).
    // Returns a list of moves that occurred (IL placements, call-ups, etc.)
    std::vector<RosterMove> postGameUpdate(
        const std::string& date,
        const std::string& teamName,
        const std::vector<std::string>& appearedPlayers
    );

    // Advance all IL countdowns by `days` (call at each calendar-day tick).
    // Returns players newly eligible for activation.
    std::vector<std::string> advanceDay(const std::string& teamName, int days = 1);

    // ── Roster queries ────────────────────────────────────────────────────────

    // Get current roster snapshot for a team.
    const TeamRoster* roster(const std::string& teamName) const;

    // Get all injured players league-wide.
    std::vector<std::pair<std::string, ActivePlayer>> allInjured() const;

    // Get players due to return from IL in the next N days.
    std::vector<std::pair<std::string, ActivePlayer>> returningWithin(
        const std::string& teamName, int days) const;

    // Is the team's active roster below 26? (triggers auto call-up logic)
    bool needsCallUp(const std::string& teamName) const;

    // ── Manual moves ─────────────────────────────────────────────────────────

    // Place a player on the IL.
    RosterMove placeOnIL(const std::string& teamName,
                         const std::string& playerName,
                         const Injury& injury,
                         const std::string& date);

    // Activate a player from the IL (player must be eligible).
    RosterMove activateFromIL(const std::string& teamName,
                              const std::string& playerName,
                              const std::string& date);

    // Option a player to the minors.
    RosterMove optionToMinors(const std::string& teamName,
                              const std::string& playerName,
                              const std::string& date);

    // Recall a player from the minors.
    RosterMove recallFromMinors(const std::string& teamName,
                                const std::string& minorPlayerName,
                                const std::string& date);

    // Seed the random engine (for reproducible seasons).
    void seed(uint32_t s) { rng_.seed(s); }

    // Register a callback for when a significant roster move occurs.
    using MoveCallback = std::function<void(const RosterMove&)>;
    void onMove(MoveCallback cb) { moveCallback_ = std::move(cb); }

private:
    InjuryRiskConfig              riskConfig_;
    std::vector<TeamRoster>       rosters_;
    std::mt19937                  rng_{42};
    MoveCallback                  moveCallback_;

    TeamRoster* findRoster(const std::string& teamName);
    const TeamRoster* findRoster(const std::string& teamName) const;
    ActivePlayer* findPlayer(TeamRoster& r, const std::string& name);

    bool rollInjury(const ActivePlayer& player, InjurySeverity& outSeverity);
    Injury generateInjury(const ActivePlayer& player, InjurySeverity severity,
                          const std::string& date);
    void autoCallUp(TeamRoster& roster, const std::string& date,
                    std::vector<RosterMove>& moves);
    void emitMove(const RosterMove& move);
};

} // namespace joji
