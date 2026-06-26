#pragma once

#include <optional>
#include <string>
#include <vector>

namespace joji {

// ── Injury ────────────────────────────────────────────────────────────────────

enum class InjurySeverity {
    Minor,     // 1-7 days (day-to-day)
    Moderate,  // 1-3 weeks (IL-15)
    Major,     // 1-3 months (IL-60)
    Season,    // season-ending
};

struct Injury {
    std::string        bodyPart;   // "shoulder", "hamstring", etc.
    InjurySeverity     severity    = InjurySeverity::Minor;
    int                daysRemaining = 0;
    std::string        dateOccurred; // YYYY-MM-DD
};

// ── Player status on roster ───────────────────────────────────────────────────

enum class RosterStatus {
    Active,      // on the 26-man active roster
    IL15,        // 15-day injured list
    IL60,        // 60-day injured list
    Minors,      // optioned to minor leagues
    Suspended,
};

struct ActivePlayer {
    std::string    name;
    std::string    position;   // "SP", "RP", "C", "1B", etc.
    int            age         = 27;
    RosterStatus   status      = RosterStatus::Active;
    std::optional<Injury> injury;

    // Condition: 0.70 (slump/fatigue) → 1.00 (normal) → 1.08 (hot streak)
    // Applied as a multiplier to contact/power/velocity in the engine via gameform.
    double condition    = 1.00;
    int    consecutiveRestDays = 0;  // days since last appearance
};

// ── Minor league entry ────────────────────────────────────────────────────────

struct MinorLeaguer {
    std::string name;
    std::string position;
    int         age         = 22;
    int         potential   = 50; // scout scale 40-80: how good the player can become
    int         readiness   = 50; // how ready to contribute at top level right now
    std::optional<Injury> injury;
};

// ── Roster move log ───────────────────────────────────────────────────────────

enum class RosterMoveType {
    PlacedOnIL,
    ActivatedFromIL,
    OptionedToMinors,
    RecalledFromMinors,
    ConditionUpdate,
};

struct RosterMove {
    std::string    date;       // YYYY-MM-DD
    std::string    playerName;
    RosterMoveType moveType;
    std::string    notes;      // "hamstring strain, 15-day IL", etc.
};

// ── Team roster snapshot ──────────────────────────────────────────────────────

struct TeamRoster {
    std::string               teamName;
    std::vector<ActivePlayer> active;   // up to 26 on active roster
    std::vector<MinorLeaguer> minors;   // minor league depth
    std::vector<RosterMove>   moves;    // log of moves this season
};

// ── Injury probability table ──────────────────────────────────────────────────
// Rolled once per game per player who appeared.
// Probabilities are per-game; calibrated for ~10-15 IL stints per team per season.

struct InjuryRiskConfig {
    double minorInjuryChance    = 0.0020; // ~16% per season if plays every game
    double moderateInjuryChance = 0.0006; // ~5%
    double majorInjuryChance    = 0.0002; // ~2%
    double seasonEndingChance   = 0.00005; // ~0.4%

    // Modifiers applied to base chances
    double pitcherMultiplier    = 1.30;  // pitchers injure more often
    double catcherMultiplier    = 1.20;
    double ageModifier          = 0.04;  // +4% per year over 30
};

} // namespace joji
