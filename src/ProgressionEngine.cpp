#include "ProgressionEngine.h"

#include "Random.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace joji {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Rating points gained per point of readiness when a rookie develops.  The draft builds a rookie
// at 0.8 points per tier; growth is deliberately a little slower, because the best arms of every
// draft class reach the rotations and the league's talent otherwise creeps up each year
// (see docs/improvement-list.md).
constexpr double kRatingPerTier = 0.6;

double normal(Random& rng, double mean, double sd) {
    const double u1 = rng.real(1e-9, 1.0);
    const double u2 = rng.real(0.0, 1.0);
    return mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

int clampRating(double value) {
    return std::clamp(static_cast<int>(std::lround(value)), 30, 95);
}

// Average yearly change for a player who has just turned `age`.
double ageCurve(int age) {
    if (age <= 24) return 1.5;
    if (age <= 26) return 0.5;
    if (age <= 30) return 0.0;
    if (age <= 32) return -0.7;
    if (age <= 34) return -1.2;
    return -2.0;
}

struct StatRef {
    int Player::*field;
    double extra;  // added on top of the age curve
};

std::vector<StatRef> statsFor(const Player& p, int age) {
    std::vector<StatRef> stats;
    if (p.position == Position::Pitcher) {
        stats.push_back({&Player::pitchingVelocity, age >= 30 ? -0.8 : 0.0});
        stats.push_back({&Player::pitchingControl, age >= 29 ? 0.2 : 0.0});
        stats.push_back({&Player::pitchingStuff, 0.0});
        stats.push_back({&Player::pitchingStamina, age >= 33 ? -0.5 : 0.0});
    } else {
        stats.push_back({&Player::contact, 0.0});
        stats.push_back({&Player::power, 0.0});
        stats.push_back({&Player::eye, age >= 30 ? 0.3 : 0.0});
        stats.push_back({&Player::speed, age >= 29 ? -0.8 : 0.0});
        stats.push_back({&Player::fielding, age >= 31 ? -0.4 : 0.0});
        stats.push_back({&Player::arm, age >= 32 ? -0.4 : 0.0});
    }
    return stats;
}

bool isDeveloping(const Player& p) { return p.potential > p.readiness; }

Player progressed(const Player& before, Random& rng) {
    Player p = before;
    p.age = before.age + 1;
    const auto stats = statsFor(p, p.age);

    if (isDeveloping(before)) {
        // Close part of the gap to the ceiling each year (about 40% plus a point of luck).
        const int gap = before.potential - before.readiness;
        const int gain = std::clamp(static_cast<int>(std::lround(gap * 0.4 + normal(rng, 1.0, 1.5))), 0, gap);
        p.readiness = before.readiness + gain;
        for (const auto& s : stats) {
            p.*(s.field) = clampRating(before.*(s.field) + kRatingPerTier * gain + normal(rng, 0.0, 1.0));
        }
        if (p.readiness >= p.potential) p.potential = 0;  // reached the ceiling: age curve from now on
        return p;
    }

    const double shared = normal(rng, 0.0, 0.8);  // a good or bad year for the whole player
    for (const auto& s : stats) {
        const double delta = ageCurve(p.age) + s.extra + shared + normal(rng, 0.0, 0.8);
        p.*(s.field) = clampRating(before.*(s.field) + delta);
    }
    return p;
}

} // namespace

std::vector<Team> advanceOneYear(const std::vector<Team>& teams,
                                 const ProgressionConfig& config,
                                 ProgressionResult& result,
                                 const std::set<std::string>& skip) {
    result.season = config.season;
    result.seed = config.seed;
    result.changes.clear();

    std::vector<Team> updated;
    updated.reserve(teams.size());

    for (std::size_t t = 0; t < teams.size(); ++t) {
        const Team& team = teams[t];
        Random rng{std::optional<std::uint32_t>{config.seed * 7919u + static_cast<std::uint32_t>(t) * 104729u + 17u}};

        auto advance = [&](const std::vector<Player>& group) {
            std::vector<Player> out;
            out.reserve(group.size());
            for (const Player& player : group) {
                if (skip.count(player.name)) {
                    out.push_back(player);
                    continue;
                }
                Player next = progressed(player, rng);
                ProgressionChange change;
                change.team = team.name();
                change.player = player.name;
                change.isPitcher = player.position == Position::Pitcher;
                change.ageBefore = player.age;
                change.ageAfter = next.age;
                change.overallBefore = playerOverall(player);
                change.overallAfter = playerOverall(next);
                change.developing = isDeveloping(player);
                result.changes.push_back(change);
                out.push_back(next);
            }
            return out;
        };

        // The order is fixed so the random stream is reproducible.
        std::vector<Player> lineup = advance(team.lineup());
        std::vector<Player> rotation = advance(team.rotation());
        std::vector<Player> bullpen = advance(team.bullpen());
        std::vector<Player> bench = advance(team.bench());
        updated.emplace_back(team.name(), std::move(lineup), std::move(rotation), std::move(bullpen),
                             std::move(bench), team.homeBallpark(), team.league());
    }
    return updated;
}

// Lets a clearly better bench hitter / bullpen arm take over a starting job (see DepthMove).
std::vector<Team> reshuffleDepth(const std::vector<Team>& teams, ProgressionResult& result) {
    std::vector<Team> updated;
    updated.reserve(teams.size());
    for (const Team& team : teams) {
        std::vector<Player> lineup = team.lineup();
        std::vector<Player> rotation = team.rotation();
        std::vector<Player> bullpen = team.bullpen();
        std::vector<Player> bench = team.bench();

        for (std::size_t i = 0; i < lineup.size(); ++i) {
            const int starterOverall = playerOverall(lineup[i]);
            int best = -1;
            int bestOverall = starterOverall + kDepthMargin - 1;
            for (std::size_t j = 0; j < bench.size(); ++j) {
                if (bench[j].position != lineup[i].position) continue;
                const int overall = playerOverall(bench[j]);
                if (overall > bestOverall) {
                    bestOverall = overall;
                    best = static_cast<int>(j);
                }
            }
            if (best < 0) continue;
            Player incoming = bench[static_cast<std::size_t>(best)];
            Player outgoing = lineup[i];
            std::swap(incoming.role, outgoing.role);  // jobs stay with the slot, names move
            result.moves.push_back({team.name(), "lineup", incoming.name, outgoing.name, bestOverall, starterOverall});
            lineup[i] = incoming;
            bench[static_cast<std::size_t>(best)] = outgoing;
        }

        for (std::size_t k = 0; k < rotation.size(); ++k) {
            const int starterOverall = playerOverall(rotation[k]);
            int best = -1;
            int bestOverall = starterOverall + kDepthMargin - 1;
            for (std::size_t j = 0; j < bullpen.size(); ++j) {
                if (bullpen[j].pitchingStamina < kStarterMinStamina) continue;  // a reliever cannot go deep
                const int overall = pitcherOverallAs(bullpen[j], PitcherRole::Starter);
                if (overall > bestOverall) {
                    bestOverall = overall;
                    best = static_cast<int>(j);
                }
            }
            if (best < 0) continue;
            Player incoming = bullpen[static_cast<std::size_t>(best)];
            Player outgoing = rotation[k];
            std::swap(incoming.role, outgoing.role);
            std::swap(incoming.pitcherRole, outgoing.pitcherRole);
            result.moves.push_back({team.name(), "rotation", incoming.name, outgoing.name, bestOverall, starterOverall});
            rotation[k] = incoming;
            bullpen[static_cast<std::size_t>(best)] = outgoing;
        }

        updated.emplace_back(team.name(), std::move(lineup), std::move(rotation), std::move(bullpen),
                             std::move(bench), team.homeBallpark(), team.league());
    }
    return updated;
}

SeasonRosters buildSeasonRosters(const std::vector<Team>& base, int season, const SeasonRosterOptions& options) {
    SeasonRosters out;
    out.teams = base;
    for (int year = kBaseSeason + 1; year <= season; ++year) {
        DraftConfig draftConfig;
        draftConfig.season = year;
        draftConfig.rounds = options.rounds;
        draftConfig.promotionMargin = options.promotionMargin;
        draftConfig.seed = (year == season && options.draftSeed) ? *options.draftSeed : static_cast<unsigned>(year);

        DraftResult draft = runDraft(out.teams, draftConfig);
        std::vector<Team> drafted = applyDraftToRosters(out.teams, draft, draftConfig);

        // Rookies who just made a roster join at their draft ratings; everyone else ages.
        std::set<std::string> newRookies;
        for (const auto& change : draft.changes) {
            if (change.type == RosterChangeType::Promoted) newRookies.insert(change.player);
        }
        ProgressionConfig progressionConfig;
        progressionConfig.season = year;
        progressionConfig.seed = static_cast<unsigned>(year) * 31u + 7u;
        ProgressionResult progression;
        std::vector<Team> aged = advanceOneYear(drafted, progressionConfig, progression, newRookies);
        out.teams = reshuffleDepth(aged, progression);

        if (year == season) {
            out.draft = std::move(draft);
            out.progression = std::move(progression);
        }
    }
    return out;
}

} // namespace joji
