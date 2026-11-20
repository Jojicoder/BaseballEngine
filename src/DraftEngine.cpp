#include "DraftEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace joji {

namespace {

constexpr double kPi = 3.14159265358979323846;

int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(hi, value));
}

// Box-Muller.  Two uniform draws per sample keeps the stream layout simple.
double normal(Random& rng, double mean, double sd) {
    const double u1 = rng.real(1e-9, 1.0);
    const double u2 = rng.real(0.0, 1.0);
    return mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

template <typename T>
const T& pickOne(Random& rng, const std::vector<T>& items) {
    return items[static_cast<std::size_t>(rng.integer(0, static_cast<int>(items.size()) - 1))];
}

// ── Templates (targets from PLAYER_ARCHETYPES.md) ─────────────────────────────

struct HitterTemplate {
    PlayerRole role;
    std::vector<Position> positions;
    std::array<int, 6> stats;  // contact, power, eye, speed, fielding, arm
    int weight;
};

struct PitcherTemplate {
    PlayerRole role;
    PitcherRole pitcherRole;
    std::array<int, 4> stats;  // velocity, control, stuff, stamina
    int weight;
};

using P = Position;
using PL = PlayerRole;

const std::vector<HitterTemplate>& hitterTemplates() {
    static const std::vector<HitterTemplate> templates = {
        {PL::Leadoff,       {P::Shortstop, P::SecondBase, P::CenterField, P::LeftField, P::RightField}, {76, 60, 72, 80, 70, 60}, 10},
        {PL::ContactHitter, {P::FirstBase, P::SecondBase, P::ThirdBase, P::LeftField, P::RightField, P::Shortstop}, {74, 68, 64, 64, 66, 64}, 12},
        {PL::PowerHitter,   {P::FirstBase, P::ThirdBase, P::LeftField, P::RightField}, {66, 84, 58, 48, 60, 62}, 10},
        {PL::CornerIF,      {P::FirstBase}, {70, 76, 62, 48, 62, 60}, 6},
        {PL::CornerIF,      {P::ThirdBase}, {74, 70, 66, 62, 70, 66}, 6},
        {PL::MiddleIF,      {P::SecondBase}, {66, 58, 64, 72, 76, 66}, 6},
        {PL::MiddleIF,      {P::Shortstop}, {68, 56, 64, 74, 80, 76}, 6},
        {PL::CenterFielder, {P::CenterField}, {74, 60, 70, 84, 78, 62}, 7},
        {PL::CornerOF,      {P::LeftField}, {68, 76, 58, 58, 64, 64}, 5},
        {PL::CornerOF,      {P::RightField}, {68, 74, 56, 58, 66, 72}, 5},
        {PL::Catcher,       {P::Catcher}, {64, 56, 64, 52, 72, 80}, 8},
        {PL::UtilityIF,     {P::SecondBase, P::Shortstop, P::ThirdBase}, {62, 54, 62, 68, 76, 72}, 5},
        {PL::ExtraOF,       {P::LeftField, P::CenterField, P::RightField}, {60, 52, 60, 76, 70, 60}, 4},
        {PL::PinchHitter,   {P::FirstBase, P::LeftField, P::RightField}, {70, 64, 66, 46, 52, 55}, 3},
        {PL::BackupCatcher, {P::Catcher}, {56, 50, 58, 46, 68, 76}, 4},
    };
    return templates;
}

// Weights are doubled vs. hitters so the pool is roughly half pitchers, like a roster.
const std::vector<PitcherTemplate>& pitcherTemplates() {
    static const std::vector<PitcherTemplate> templates = {
        {PL::Ace,            PitcherRole::Starter,      {86, 70, 84, 78}, 2},
        {PL::Starter,        PitcherRole::Starter,      {78, 70, 76, 72}, 16},
        {PL::BackOfRotation, PitcherRole::Starter,      {70, 64, 66, 62}, 20},
        {PL::Closer,         PitcherRole::Closer,       {86, 64, 84, 44}, 6},
        {PL::Setup,          PitcherRole::Setup,        {80, 66, 78, 52}, 12},
        {PL::MiddleRelief,   PitcherRole::MiddleRelief, {74, 64, 70, 60}, 24},
        {PL::LongRelief,     PitcherRole::LongRelief,   {70, 68, 64, 74}, 12},
        {PL::Specialist,     PitcherRole::MiddleRelief, {72, 68, 66, 52}, 8},
    };
    return templates;
}

template <typename T>
const T& weightedPick(Random& rng, const std::vector<T>& items) {
    int total = 0;
    for (const auto& item : items) total += item.weight;
    int roll = rng.integer(1, total);
    for (const auto& item : items) {
        roll -= item.weight;
        if (roll <= 0) return item;
    }
    return items.back();
}

const std::vector<std::string>& firstNames() {
    static const std::vector<std::string> names = {
        "Aaron", "Adrian", "Alex", "Andre", "Andy", "Anton", "Archie", "Ben", "Blake", "Bobby",
        "Brad", "Brent", "Caleb", "Carlos", "Cecil", "Chad", "Chris", "Cody", "Colin", "Craig",
        "Dale", "Damon", "Danny", "Darius", "Dean", "Dillon", "Dom", "Drew", "Dustin", "Eddie",
        "Elijah", "Emilio", "Eric", "Ethan", "Evan", "Felipe", "Frank", "Gabe", "Gary", "Glenn",
        "Grant", "Gus", "Hank", "Harlan", "Hector", "Henry", "Hugo", "Ian", "Isaac", "Jace",
        "Jamal", "Jared", "Jason", "Jay", "Jesse", "Joel", "Jonah", "Jorge", "Josh", "Julian",
        "Keith", "Kenny", "Kevin", "Kyle", "Lance", "Leo", "Liam", "Logan", "Lucas", "Luke",
        "Malik", "Marco", "Mason", "Miles", "Nate", "Nick", "Noah", "Omar", "Owen", "Pablo",
        "Quinn", "Ray", "Reid", "Ricky", "Ryan", "Sam", "Seth", "Shane", "Tate", "Theo",
        "Tomas", "Trent", "Troy", "Vince", "Wade", "Wes", "Will", "Xavier", "Zach", "Zane",
    };
    return names;
}

const std::vector<std::string>& lastNames() {
    static const std::vector<std::string> names = {
        "Abbott", "Acosta", "Adler", "Avery", "Baker", "Banks", "Barnes", "Bates", "Beck", "Bishop",
        "Boone", "Bowen", "Brandt", "Briggs", "Burke", "Byrd", "Cahill", "Carver", "Chase", "Cole",
        "Conway", "Cross", "Dalton", "Dawson", "Decker", "Dixon", "Doyle", "Drake", "Duffy", "Ellis",
        "Emery", "Farley", "Finch", "Fisher", "Flynn", "Foley", "Frost", "Gaines", "Garner", "Gibbs",
        "Gomez", "Hale", "Hardy", "Hayes", "Heath", "Hicks", "Holt", "Hood", "Hunt", "Irwin",
        "Jarvis", "Keane", "Kemp", "Knox", "Lamb", "Lane", "Lowe", "Mack", "Marsh", "Maddox",
        "Nash", "Nolan", "Norris", "Oakes", "Pace", "Parrish", "Pike", "Poole", "Quade", "Rand",
        "Reese", "Rhodes", "Rowe", "Rudd", "Sanders", "Shaw", "Slater", "Stokes", "Sutton", "Tanner",
        "Thorne", "Tucker", "Vance", "Voss", "Walsh", "Ward", "Watts", "West", "Yates", "Young",
    };
    return names;
}

std::string makeName(Random& rng, std::set<std::string>& used) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        std::string name = pickOne(rng, firstNames()) + " " + pickOne(rng, lastNames());
        if (used.insert(name).second) return name;
    }
    // Pool is far smaller than first*last combinations, so this is effectively unreachable.
    std::string fallback = "Rookie " + std::to_string(used.size() + 1);
    used.insert(fallback);
    return fallback;
}

int rateStat(Random& rng, int target, int readiness) {
    // 62 readiness = the archetype's typical regular; each point shifts ratings by 0.8.
    const double value = target + (readiness - 62) * 0.8 + normal(rng, 0.0, 3.5);
    return clampInt(static_cast<int>(std::lround(value)), 30, 95);
}

std::vector<PitchGrade> makeArsenal(Random& rng, const Player& pitcher) {
    std::vector<PitchType> secondary = {PitchType::Slider, PitchType::Curveball, PitchType::Changeup,
                                        PitchType::Cutter, PitchType::Splitter};
    for (std::size_t i = secondary.size() - 1; i > 0; --i) {
        std::swap(secondary[i], secondary[static_cast<std::size_t>(rng.integer(0, static_cast<int>(i)))]);
    }
    const bool starter = pitcher.pitcherRole == PitcherRole::Starter || pitcher.pitcherRole == PitcherRole::LongRelief;
    const int extras = starter ? rng.integer(2, 3) : rng.integer(1, 2);

    std::vector<PitchGrade> arsenal;
    arsenal.push_back({PitchType::Fastball,
                       clampInt(pitcher.pitchingVelocity - 6 + rng.integer(-2, 2), 40, 90)});
    int grade = pitcher.pitchingStuff - 6;
    for (int i = 0; i < extras; ++i) {
        arsenal.push_back({secondary[static_cast<std::size_t>(i)],
                           clampInt(grade + rng.integer(-3, 3), 40, 90)});
        grade -= 6;
    }
    return arsenal;
}

int pickAge(Random& rng) {
    static const std::array<std::pair<int, int>, 5> weights = {{{18, 10}, {19, 20}, {20, 25}, {21, 25}, {22, 20}}};
    int roll = rng.integer(1, 100);
    for (const auto& [age, weight] : weights) {
        roll -= weight;
        if (roll <= 0) return age;
    }
    return 22;
}

DraftProspect makeProspect(int id, Random& rng, std::set<std::string>& used) {
    DraftProspect prospect;
    prospect.id = id;
    prospect.name = makeName(rng, used);
    prospect.age = pickAge(rng);
    prospect.readiness = clampInt(static_cast<int>(std::lround(normal(rng, 52.0, 8.0))), 30, 80);
    prospect.potential = clampInt(prospect.readiness + rng.integer(6, 18), prospect.readiness, 85);

    Player p;
    p.name = prospect.name;
    p.age = prospect.age;

    const bool isPitcher = rng.integer(1, 100) <= 50;
    if (isPitcher) {
        const PitcherTemplate& t = weightedPick(rng, pitcherTemplates());
        p.position = Position::Pitcher;
        p.pitchingVelocity = rateStat(rng, t.stats[0], prospect.readiness);
        p.pitchingControl = rateStat(rng, t.stats[1], prospect.readiness);
        p.pitchingStuff = rateStat(rng, t.stats[2], prospect.readiness);
        p.pitchingStamina = rateStat(rng, t.stats[3], prospect.readiness);
        p.pitcherRole = t.pitcherRole;
        p.role = t.role;
        p.throwingHand = rng.integer(1, 100) <= 78 ? ThrowingHand::Right : ThrowingHand::Left;
        // Same minimal batting line as the hand-authored pitchers in Teams.cpp.
        p.contact = 42; p.power = 40; p.eye = 40; p.speed = 40;
        p.fielding = 52; p.arm = 58;
        p.arsenal = makeArsenal(rng, p);
    } else {
        const HitterTemplate& t = weightedPick(rng, hitterTemplates());
        p.position = pickOne(rng, t.positions);
        p.contact = rateStat(rng, t.stats[0], prospect.readiness);
        p.power = rateStat(rng, t.stats[1], prospect.readiness);
        p.eye = rateStat(rng, t.stats[2], prospect.readiness);
        p.speed = rateStat(rng, t.stats[3], prospect.readiness);
        p.fielding = rateStat(rng, t.stats[4], prospect.readiness);
        p.arm = rateStat(rng, t.stats[5], prospect.readiness);
        p.role = t.role;
        const int sideRoll = rng.integer(1, 100);
        p.battingSide = sideRoll <= 60 ? BattingSide::Right : (sideRoll <= 93 ? BattingSide::Left : BattingSide::Switch);
        p.throwingHand = rng.integer(1, 100) <= 88 ? ThrowingHand::Right : ThrowingHand::Left;
        p.pullTendency = clampInt(50 + static_cast<int>(std::lround(normal(rng, 0.0, 9.0))), 30, 80);
        p.highBallHitter = clampInt(50 + static_cast<int>(std::lround(normal(rng, 0.0, 7.0))), 35, 70);
        p.chaseRate = clampInt(static_cast<int>(std::lround(108.0 - p.eye + normal(rng, 0.0, 5.0))), 25, 75);
        p.contactVsBreaking = clampInt(static_cast<int>(std::lround(p.contact * 0.7 + normal(rng, 0.0, 5.0))), 30, 70);
    }
    p.clutchRating = clampInt(50 + static_cast<int>(std::lround(normal(rng, 0.0, 8.0))), 30, 75);

    prospect.position = p.position;
    prospect.role = p.role;
    prospect.asPlayer = p;
    prospect.overall = playerOverall(p);
    return prospect;
}

} // namespace

// ── Public ────────────────────────────────────────────────────────────────────

int pitcherOverallAs(const Player& player, PitcherRole role) {
    double staminaWeight = 0.2;
    if (role == PitcherRole::Starter) staminaWeight = 0.7;
    else if (role == PitcherRole::LongRelief) staminaWeight = 0.5;
    const double total = 0.8 + 1.0 + 1.2 + staminaWeight;
    const double sum = player.pitchingVelocity * 0.8 + player.pitchingControl * 1.0 +
                       player.pitchingStuff * 1.2 + player.pitchingStamina * staminaWeight;
    return static_cast<int>(std::lround(sum / total));
}

int playerOverall(const Player& player) {
    if (player.position == Position::Pitcher) {
        return pitcherOverallAs(player, player.pitcherRole);
    }
    const double sum = player.contact * 1.0 + player.power * 0.9 + player.eye * 0.7 +
                       player.speed * 0.5 + player.fielding * 0.6 + player.arm * 0.3;
    return static_cast<int>(std::lround(sum / 4.0));
}

DraftStrategy randomStrategy() {
    return [](const std::vector<DraftProspect>& available, const Team&, Random& rng) {
        return static_cast<std::size_t>(rng.integer(0, static_cast<int>(available.size()) - 1));
    };
}

std::vector<DraftProspect> generateProspectPool(int count,
                                                Random& rng,
                                                const std::vector<std::string>& takenNames) {
    std::set<std::string> used(takenNames.begin(), takenNames.end());
    std::vector<DraftProspect> pool;
    pool.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i) {
        pool.push_back(makeProspect(i + 1, rng, used));
    }
    return pool;
}

DraftResult runDraft(const std::vector<Team>& teams,
                     const DraftConfig& config,
                     const DraftStrategy& strategy) {
    DraftResult result;
    result.season = config.season;
    result.seed = config.seed;
    result.rounds = config.rounds;

    Random rng{std::optional<std::uint32_t>{config.seed}};

    std::vector<std::string> taken;
    for (const auto& team : teams) {
        for (const auto& p : team.lineup()) taken.push_back(p.name);
        for (const auto& p : team.rotation()) taken.push_back(p.name);
        for (const auto& p : team.bullpen()) taken.push_back(p.name);
        for (const auto& p : team.bench()) taken.push_back(p.name);
    }

    const int teamCount = static_cast<int>(teams.size());
    const int needed = teamCount * config.rounds;
    const int poolSize = static_cast<int>(std::ceil(needed * config.poolMultiplier));
    result.pool = generateProspectPool(std::max(poolSize, needed), rng, taken);

    // Draft order: shuffled once, then snake.
    std::vector<int> order(static_cast<std::size_t>(teamCount));
    for (int i = 0; i < teamCount; ++i) order[static_cast<std::size_t>(i)] = i;
    for (int i = teamCount - 1; i > 0; --i) {
        std::swap(order[static_cast<std::size_t>(i)], order[static_cast<std::size_t>(rng.integer(0, i))]);
    }

    std::vector<DraftProspect> available = result.pool;
    int overallPick = 0;
    for (int round = 1; round <= config.rounds; ++round) {
        for (int slot = 0; slot < teamCount; ++slot) {
            const int teamIdx = (round % 2 == 1) ? order[static_cast<std::size_t>(slot)]
                                                 : order[static_cast<std::size_t>(teamCount - 1 - slot)];
            const Team& team = teams[static_cast<std::size_t>(teamIdx)];
            const std::size_t chosen = strategy(available, team, rng);

            DraftPick pick;
            pick.round = round;
            pick.pickInRound = slot + 1;
            pick.overall = ++overallPick;
            pick.teamIdx = teamIdx;
            pick.team = team.name();
            pick.prospectId = available[chosen].id;
            result.picks.push_back(pick);

            available.erase(available.begin() + static_cast<std::ptrdiff_t>(chosen));
        }
    }
    return result;
}

std::vector<Team> applyDraftToRosters(const std::vector<Team>& teams,
                                      DraftResult& result,
                                      const DraftConfig& config) {
    result.changes.clear();
    std::vector<Team> updated;
    updated.reserve(teams.size());

    for (std::size_t t = 0; t < teams.size(); ++t) {
        const Team& team = teams[t];

        std::vector<const DraftProspect*> drafted;
        for (const auto& pick : result.picks) {
            if (pick.teamIdx != static_cast<int>(t)) continue;
            for (const auto& prospect : result.pool) {
                if (prospect.id == pick.prospectId) drafted.push_back(&prospect);
            }
        }
        // Best rookies get first call on the weakest spots.
        std::sort(drafted.begin(), drafted.end(), [](const DraftProspect* a, const DraftProspect* b) {
            return a->overall != b->overall ? a->overall > b->overall : a->id < b->id;
        });

        std::vector<Player> bench = team.bench();
        std::vector<Player> bullpen = team.bullpen();
        std::vector<bool> benchTaken(bench.size(), false);
        std::vector<bool> bullpenTaken(bullpen.size(), false);

        for (const DraftProspect* prospect : drafted) {
            const bool isPitcher = prospect->position == Position::Pitcher;
            std::vector<Player>& slotPlayers = isPitcher ? bullpen : bench;
            std::vector<bool>& taken = isPitcher ? bullpenTaken : benchTaken;

            // Weakest not-yet-replaced player, judged in their own role.
            int weakest = -1;
            int weakestOverall = 0;
            for (std::size_t i = 0; i < slotPlayers.size(); ++i) {
                if (taken[i]) continue;
                const int overall = playerOverall(slotPlayers[i]);
                if (weakest < 0 || overall < weakestOverall) {
                    weakest = static_cast<int>(i);
                    weakestOverall = overall;
                }
            }

            bool promoted = false;
            if (weakest >= 0) {
                const Player& incumbent = slotPlayers[static_cast<std::size_t>(weakest)];
                // A pitcher takes over the incumbent's role, so the comparison uses that role.
                const int rookieOverall = isPitcher
                    ? pitcherOverallAs(prospect->asPlayer, incumbent.pitcherRole)
                    : prospect->overall;
                if (rookieOverall >= weakestOverall + config.promotionMargin) {
                    Player rookie = prospect->asPlayer;
                    if (isPitcher) {
                        rookie.pitcherRole = incumbent.pitcherRole;
                        rookie.role = incumbent.role;
                    } else {
                        rookie.role = incumbent.role;
                    }

                    RosterChange promotedChange;
                    promotedChange.team = team.name();
                    promotedChange.type = RosterChangeType::Promoted;
                    promotedChange.player = rookie.name;
                    promotedChange.prospectId = prospect->id;
                    promotedChange.slot = isPitcher ? "bullpen" : "bench";
                    promotedChange.replaced = incumbent.name;
                    promotedChange.overall = rookieOverall;
                    result.changes.push_back(promotedChange);

                    RosterChange releasedChange;
                    releasedChange.team = team.name();
                    releasedChange.type = RosterChangeType::Released;
                    releasedChange.player = incumbent.name;
                    releasedChange.slot = promotedChange.slot;
                    releasedChange.overall = weakestOverall;
                    result.changes.push_back(releasedChange);

                    slotPlayers[static_cast<std::size_t>(weakest)] = rookie;
                    taken[static_cast<std::size_t>(weakest)] = true;
                    promoted = true;
                }
            }

            if (!promoted) {
                RosterChange minorsChange;
                minorsChange.team = team.name();
                minorsChange.type = RosterChangeType::Minors;
                minorsChange.player = prospect->name;
                minorsChange.prospectId = prospect->id;
                minorsChange.overall = prospect->overall;
                result.changes.push_back(minorsChange);
            }
        }

        updated.emplace_back(team.name(), team.lineup(), team.rotation(), bullpen, bench,
                             team.homeBallpark(), team.league());
    }
    return updated;
}

} // namespace joji
