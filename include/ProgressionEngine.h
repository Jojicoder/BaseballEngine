#pragma once

#include "DraftEngine.h"
#include "Player.h"
#include "Team.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace joji {

// ── Aging & rookie development (roadmap phase 5) ──────────────────────────────
//
// Each offseason every player gets one year older and his ratings move:
//   * Developing players (rookies from the draft: Player::potential > Player::readiness)
//     grow toward their ceiling, closing about 40% of the gap each year.
//   * Everyone else follows an age curve: young players still improve a little, the
//     prime years are flat, then ratings slide.  Speed and velocity go first.
//
// After the ratings move, a bench hitter (same position) or bullpen arm who is clearly better
// than a starter (by `kDepthMargin` overall) takes the job, so a developed rookie can win a
// starting spot and the league does not just wear out as its regulars age.
//
// Retirement (see retirementChance): from 34 on, a player may retire after the ratings move; the
// older and weaker, the likelier.  His starting job goes to the best bench/bullpen player, and the
// vacated bench/bullpen spot is filled by a prospect drafted that year who did not make a roster
// (the team's own first, then the undrafted pool).  Roster sizes never change.
//
// Rookies who join this offseason do not age or grow in the year they arrive.
// Everything is driven by ProgressionConfig::seed, so the same inputs always give
// the same result (no std::*_distribution: output is identical on every platform).

struct ProgressionConfig {
    int season = 2027;   // the season the new ratings are for
    unsigned seed = 1;
};

struct ProgressionChange {
    std::string team;
    std::string player;
    bool isPitcher = false;
    int ageBefore = 0;
    int ageAfter = 0;
    int overallBefore = 0;
    int overallAfter = 0;
    bool developing = false;  // still growing toward a ceiling (a drafted rookie)
};

// A bench/bullpen player who took a starting job after the ratings were updated.
struct DepthMove {
    std::string team;
    std::string slot;      // "lineup" | "rotation"
    std::string promoted;  // now starts
    std::string demoted;   // moved to the bench / bullpen
    int promotedOverall = 0;
    int demotedOverall = 0;
};

// A player who left the game after aging.  `replacement` is the drafted prospect who filled the
// vacated roster spot (after any bench/bullpen player took over the starting job).
struct Retirement {
    std::string team;
    std::string player;
    bool isPitcher = false;
    int age = 0;
    int overall = 0;
    std::string slot;         // "lineup" | "rotation" | "bullpen" | "bench"
    std::string replacement;  // the prospect who joined the roster
};

struct ProgressionResult {
    int season = 0;
    unsigned seed = 0;
    std::vector<ProgressionChange> changes;
    std::vector<DepthMove> moves;
    std::vector<Retirement> retirements;
};

// Returns the teams one year later.  Roster sizes never change.  Players whose name is in
// `skip` (this offseason's new rookies) are copied unchanged.  Fills result.changes.
std::vector<Team> advanceOneYear(const std::vector<Team>& teams,
                                 const ProgressionConfig& config,
                                 ProgressionResult& result,
                                 const std::set<std::string>& skip = {});

// ── Season rosters ───────────────────────────────────────────────────────────
//
// The hand-written rosters in Teams.cpp are the 2026 rosters.  Season S > 2026 is built
// by replaying every offseason from 2027 to S:  draft (seed = the year) -> rookies take
// bench/bullpen spots -> everyone ages one year.

constexpr int kBaseSeason = 2026;
constexpr int kDepthMargin = 3;
constexpr int kStarterMinStamina = 60;  // minimum stamina for a bullpen arm to move into the rotation

struct SeasonRosterOptions {
    int rounds = 7;
    int promotionMargin = 5;
    std::optional<unsigned> draftSeed;  // overrides the draft seed of the target season only
};

struct SeasonRosters {
    std::vector<Team> teams;         // the rosters the season is played with
    DraftResult draft;               // the target season's draft (empty for seasons <= 2026)
    ProgressionResult progression;   // the target season's aging (empty for seasons <= 2026)
};

// Chance that a player of `age` and `overall` retires this offseason.
double retirementChance(int age, int overall);

SeasonRosters buildSeasonRosters(const std::vector<Team>& base, int season,
                                 const SeasonRosterOptions& options = {});

} // namespace joji
