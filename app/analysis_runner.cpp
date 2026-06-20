#include "GameEngine.h"
#include "Teams.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int GamesPerSeries = 3;

struct AnalysisMode {
    std::string name = "100-seasons";
    int games = 0;
    int seasons = 100;
};

struct TeamAnalysis {
    std::string name;
    int wins = 0;
    int losses = 0;
    int runsFor = 0;
    int runsAgainst = 0;
    int games = 0;

    int atBats = 0;
    int hits = 0;
    int walks = 0;
    int strikeouts = 0;
    int homeRuns = 0;
    int totalBases = 0;
    int sacFlies = 0;

    int extraBasesTaken = 0;
    int thrownOutAtHome = 0;
    int sacFlyAttempts = 0;
    int sacFlySuccesses = 0;
    int homeOutfieldAssists = 0;

    int plateAppearances() const { return atBats + walks + sacFlies; }
    double avg() const { return atBats > 0 ? static_cast<double>(hits) / atBats : 0.0; }
    double obp() const {
        const int denom = atBats + walks + sacFlies;
        return denom > 0 ? static_cast<double>(hits + walks) / denom : 0.0;
    }
    double slg() const { return atBats > 0 ? static_cast<double>(totalBases) / atBats : 0.0; }
    double ops() const { return obp() + slg(); }
    double kPct() const {
        const int pa = plateAppearances();
        return pa > 0 ? strikeouts * 100.0 / pa : 0.0;
    }
    double bbPct() const {
        const int pa = plateAppearances();
        return pa > 0 ? walks * 100.0 / pa : 0.0;
    }
    double hrPct() const {
        const int pa = plateAppearances();
        return pa > 0 ? homeRuns * 100.0 / pa : 0.0;
    }
    double runsPerGame() const { return games > 0 ? static_cast<double>(runsFor) / games : 0.0; }
    double runsAllowedPerGame() const { return games > 0 ? static_cast<double>(runsAgainst) / games : 0.0; }
    double xbtPerGame() const { return games > 0 ? static_cast<double>(extraBasesTaken) / games : 0.0; }
    double tohPerGame() const { return games > 0 ? static_cast<double>(thrownOutAtHome) / games : 0.0; }
    double sfPct() const { return sacFlyAttempts > 0 ? sacFlySuccesses * 100.0 / sacFlyAttempts : 0.0; }
    double hOfaPerGame() const { return games > 0 ? static_cast<double>(homeOutfieldAssists) / games : 0.0; }
};

AnalysisMode parseMode(int argc, char* argv[]) {
    const std::string arg = argc >= 2 ? argv[1] : "100-seasons";
    if (arg == "100-games") return {"100-games", 100, 0};
    if (arg == "100-seasons") return {"100-seasons", 0, 100};
    if (arg == "1000-seasons") return {"1000-seasons", 0, 1000};

    std::cerr << "Unknown mode: " << arg << "\n"
              << "Usage: JojiAnalysisRunner [100-games|100-seasons|1000-seasons]\n";
    std::exit(2);
}

std::string fixed(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

void accumulateOffense(TeamAnalysis& team, const joji::TeamBoxScore& box) {
    team.atBats += box.atBats;
    team.hits += box.hits;
    team.walks += box.walks;
    team.strikeouts += box.strikeouts;
    team.homeRuns += box.homeRuns;
    team.totalBases += box.totalBases;
    team.sacFlies += box.sacFlySuccesses;
    team.extraBasesTaken += box.extraBasesTaken;
    team.thrownOutAtHome += box.runnersThrownOutAtHome;
    team.sacFlyAttempts += box.sacFlyAttempts;
    team.sacFlySuccesses += box.sacFlySuccesses;
}

void accumulateDefense(TeamAnalysis& team, const std::vector<joji::PlayerBoxScore>& players) {
    for (const auto& player : players) {
        team.homeOutfieldAssists += player.outfieldAssists;
    }
}

void accumulateGame(TeamAnalysis& away,
                    TeamAnalysis& home,
                    const joji::GameEngine& engine) {
    const joji::GameResult result = engine.result();
    const bool awayWon = result.type == joji::GameResultType::AwayWin;
    const bool homeWon = result.type == joji::GameResultType::HomeWin;

    away.wins += awayWon ? 1 : 0;
    away.losses += homeWon ? 1 : 0;
    home.wins += homeWon ? 1 : 0;
    home.losses += awayWon ? 1 : 0;

    away.runsFor += result.awayScore;
    away.runsAgainst += result.homeScore;
    home.runsFor += result.homeScore;
    home.runsAgainst += result.awayScore;
    away.games += 1;
    home.games += 1;

    accumulateOffense(away, engine.awayBoxScore());
    accumulateOffense(home, engine.homeBoxScore());
    accumulateDefense(away, engine.awayPlayerStats());
    accumulateDefense(home, engine.homePlayerStats());
}

std::vector<std::pair<int, int>> roundRobinPairs(int teamCount) {
    std::vector<std::pair<int, int>> pairs;
    for (int i = 0; i < teamCount - 1; ++i) {
        for (int j = i + 1; j < teamCount; ++j) {
            pairs.push_back({i, j});
        }
    }
    return pairs;
}

void runOneGame(const std::vector<joji::Team>& teams,
                std::vector<TeamAnalysis>& analysis,
                int awayIdx,
                int homeIdx,
                std::uint32_t& seed) {
    joji::GameEngine engine{
        teams[static_cast<std::size_t>(awayIdx)],
        teams[static_cast<std::size_t>(homeIdx)],
        joji::Random{std::optional<std::uint32_t>{seed++}}
    };
    std::ostringstream sink;
    engine.simulate(sink);
    accumulateGame(analysis[static_cast<std::size_t>(awayIdx)],
                   analysis[static_cast<std::size_t>(homeIdx)],
                   engine);
}

void runGamesMode(const std::vector<joji::Team>& teams,
                  std::vector<TeamAnalysis>& analysis,
                  int games,
                  std::uint32_t& seed) {
    const auto pairs = roundRobinPairs(static_cast<int>(teams.size()));
    for (int g = 0; g < games; ++g) {
        const auto [a, b] = pairs[static_cast<std::size_t>(g) % pairs.size()];
        const bool flip = (g / static_cast<int>(pairs.size())) % 2 == 1;
        runOneGame(teams, analysis, flip ? b : a, flip ? a : b, seed);
    }
}

void runSeasonsMode(const std::vector<joji::Team>& teams,
                    std::vector<TeamAnalysis>& analysis,
                    int seasons,
                    std::uint32_t& seed) {
    const int n = static_cast<int>(teams.size());
    for (int season = 0; season < seasons; ++season) {
        for (int i = 0; i < n - 1; ++i) {
            for (int j = i + 1; j < n; ++j) {
                for (int g = 0; g < GamesPerSeries; ++g) {
                    const int awayIdx = (g % 2 == 0) ? i : j;
                    const int homeIdx = (g % 2 == 0) ? j : i;
                    runOneGame(teams, analysis, awayIdx, homeIdx, seed);
                }
            }
        }
        if (seasons >= 100 && (season + 1) % (seasons / 10) == 0) {
            std::cerr << "  " << (season + 1) << "/" << seasons << " seasons done\r";
        }
    }
    if (seasons >= 100) std::cerr << "\n";
}

TeamAnalysis leagueTotals(const std::vector<TeamAnalysis>& teams) {
    TeamAnalysis total;
    total.name = "League";
    for (const auto& team : teams) {
        total.wins += team.wins;
        total.losses += team.losses;
        total.runsFor += team.runsFor;
        total.runsAgainst += team.runsAgainst;
        total.games += team.games;
        total.atBats += team.atBats;
        total.hits += team.hits;
        total.walks += team.walks;
        total.strikeouts += team.strikeouts;
        total.homeRuns += team.homeRuns;
        total.totalBases += team.totalBases;
        total.sacFlies += team.sacFlies;
        total.extraBasesTaken += team.extraBasesTaken;
        total.thrownOutAtHome += team.thrownOutAtHome;
        total.sacFlyAttempts += team.sacFlyAttempts;
        total.sacFlySuccesses += team.sacFlySuccesses;
        total.homeOutfieldAssists += team.homeOutfieldAssists;
    }
    return total;
}

void printLeagueAverages(const TeamAnalysis& league) {
    std::cout << "\n=== League Averages ===\n";
    std::cout << std::left << std::setw(8) << "AVG"
              << std::setw(8) << "OBP"
              << std::setw(8) << "SLG"
              << std::setw(8) << "OPS"
              << std::setw(8) << "K%"
              << std::setw(8) << "BB%"
              << std::setw(8) << "HR%"
              << std::setw(8) << "R/G"
              << "\n" << std::string(64, '-') << "\n";
    std::cout << std::left << std::setw(8) << fixed(league.avg(), 3)
              << std::setw(8) << fixed(league.obp(), 3)
              << std::setw(8) << fixed(league.slg(), 3)
              << std::setw(8) << fixed(league.ops(), 3)
              << std::setw(8) << (fixed(league.kPct(), 1) + "%")
              << std::setw(8) << (fixed(league.bbPct(), 1) + "%")
              << std::setw(8) << (fixed(league.hrPct(), 1) + "%")
              << std::setw(8) << fixed(league.runsPerGame(), 2)
              << "\n";
}

void printTeamIdentity(std::vector<TeamAnalysis> teams) {
    std::sort(teams.begin(), teams.end(), [](const TeamAnalysis& a, const TeamAnalysis& b) {
        const double ap = (a.wins + a.losses) > 0 ? static_cast<double>(a.wins) / (a.wins + a.losses) : 0.0;
        const double bp = (b.wins + b.losses) > 0 ? static_cast<double>(b.wins) / (b.wins + b.losses) : 0.0;
        return ap > bp;
    });

    std::cout << "\n=== Team Identity ===\n";
    std::cout << std::left << std::setw(22) << "Team"
              << std::right << std::setw(7) << "W"
              << std::setw(7) << "L"
              << std::setw(7) << "RF/G"
              << std::setw(7) << "RA/G"
              << std::setw(7) << "AVG"
              << std::setw(7) << "OBP"
              << std::setw(7) << "SLG"
              << std::setw(7) << "OPS"
              << std::setw(7) << "XBT/G"
              << std::setw(7) << "TOH/G"
              << std::setw(7) << "SF%"
              << std::setw(8) << "H-OFA/G"
              << "\n" << std::string(101, '-') << "\n";

    for (const auto& team : teams) {
        std::cout << std::left << std::setw(22) << team.name
                  << std::right << std::setw(7) << team.wins
                  << std::setw(7) << team.losses
                  << std::setw(7) << fixed(team.runsPerGame(), 2)
                  << std::setw(7) << fixed(team.runsAllowedPerGame(), 2)
                  << std::setw(7) << fixed(team.avg(), 3)
                  << std::setw(7) << fixed(team.obp(), 3)
                  << std::setw(7) << fixed(team.slg(), 3)
                  << std::setw(7) << fixed(team.ops(), 3)
                  << std::setw(7) << fixed(team.xbtPerGame(), 2)
                  << std::setw(7) << fixed(team.tohPerGame(), 3)
                  << std::setw(7) << (fixed(team.sfPct(), 1) + "%")
                  << std::setw(8) << fixed(team.hOfaPerGame(), 3)
                  << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const AnalysisMode mode = parseMode(argc, argv);
    const auto teams = joji::allTeams();

    std::vector<TeamAnalysis> analysis;
    analysis.reserve(teams.size());
    for (const auto& team : teams) {
        TeamAnalysis item;
        item.name = team.name();
        analysis.push_back(item);
    }

    std::cout << "Joji Baseball Engine v1.1 - Analysis Runner\n";
    std::cout << "Mode: " << mode.name << "\n" << std::flush;

    std::uint32_t seed = 424242u;
    if (mode.games > 0) {
        runGamesMode(teams, analysis, mode.games, seed);
    } else {
        runSeasonsMode(teams, analysis, mode.seasons, seed);
    }

    printLeagueAverages(leagueTotals(analysis));
    printTeamIdentity(analysis);

    return 0;
}
