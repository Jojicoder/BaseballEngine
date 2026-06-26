#include "GameEngine.h"
#include "JsonExporter.h"
#include "RunExpectancy.h"
#include "Teams.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <fstream>
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
    int runnerOutsOnBases = 0;
    int thrownOutAtHome = 0;
    int sacFlyAttempts = 0;
    int sacFlySuccesses = 0;
    int homeOutfieldAssists = 0;
    int errors = 0;
    int badThrows = 0;
    int throwingErrors = 0;
    int stolenBaseAttempts = 0;
    int stolenBases = 0;
    int caughtStealing = 0;
    int wildPitches = 0;
    int passedBalls = 0;
    int balks = 0;
    int pickoffAttempts = 0;
    int pickoffOuts = 0;
    int buntAttempts = 0;
    int buntSuccesses = 0;
    int infieldFlies = 0;

    int plateAppearances() const { return atBats + walks + sacFlies; }
    double avg() const { return atBats > 0 ? static_cast<double>(hits) / atBats : 0.0; }
    double obp() const {
        const int denom = atBats + walks + sacFlies;
        return denom > 0 ? static_cast<double>(hits + walks) / denom : 0.0;
    }
    double slg() const { return atBats > 0 ? static_cast<double>(totalBases) / atBats : 0.0; }
    double ops() const { return obp() + slg(); }
    double babip() const {
        const int denom = atBats - strikeouts - homeRuns + sacFlies;
        return denom > 0 ? static_cast<double>(hits - homeRuns) / denom : 0.0;
    }
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
    double oobPerGame() const { return games > 0 ? static_cast<double>(runnerOutsOnBases) / games : 0.0; }
    double tohPerGame() const { return games > 0 ? static_cast<double>(thrownOutAtHome) / games : 0.0; }
    double sfPct() const { return sacFlyAttempts > 0 ? sacFlySuccesses * 100.0 / sacFlyAttempts : 0.0; }
    double hOfaPerGame() const { return games > 0 ? static_cast<double>(homeOutfieldAssists) / games : 0.0; }
    double errorsPerGame() const { return games > 0 ? static_cast<double>(errors) / games : 0.0; }
    double badThrowsPerGame() const { return games > 0 ? static_cast<double>(badThrows) / games : 0.0; }
    double throwingErrorsPerGame() const { return games > 0 ? static_cast<double>(throwingErrors) / games : 0.0; }
    double sbAttemptPerGame() const { return games > 0 ? static_cast<double>(stolenBaseAttempts) / games : 0.0; }
    double sbPct() const { return stolenBaseAttempts > 0 ? stolenBases * 100.0 / stolenBaseAttempts : 0.0; }
    double csPerGame() const { return games > 0 ? static_cast<double>(caughtStealing) / games : 0.0; }
    double wpPerGame() const { return games > 0 ? static_cast<double>(wildPitches) / games : 0.0; }
    double pbPerGame() const { return games > 0 ? static_cast<double>(passedBalls) / games : 0.0; }
    double balkPerGame() const { return games > 0 ? static_cast<double>(balks) / games : 0.0; }
    double pickoffAttemptPerGame() const { return games > 0 ? static_cast<double>(pickoffAttempts) / games : 0.0; }
    double pickoffOutPerGame() const { return games > 0 ? static_cast<double>(pickoffOuts) / games : 0.0; }
    double buntAttemptPerGame() const { return games > 0 ? static_cast<double>(buntAttempts) / games : 0.0; }
    double buntSuccessPct() const { return buntAttempts > 0 ? buntSuccesses * 100.0 / buntAttempts : 0.0; }
    double iffPerGame() const { return games > 0 ? static_cast<double>(infieldFlies) / games : 0.0; }
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
    team.runnerOutsOnBases += box.runnerOutsOnBases;
    team.thrownOutAtHome += box.runnersThrownOutAtHome;
    team.sacFlyAttempts += box.sacFlyAttempts;
    team.sacFlySuccesses += box.sacFlySuccesses;
    team.errors += box.errors;
    team.badThrows += box.badThrows;
    team.throwingErrors += box.throwingErrors;
    team.stolenBaseAttempts += box.stolenBaseAttempts;
    team.stolenBases += box.stolenBases;
    team.caughtStealing += box.caughtStealing;
    team.wildPitches += box.wildPitches;
    team.passedBalls += box.passedBalls;
    team.balks += box.balks;
    team.pickoffAttempts += box.pickoffAttempts;
    team.pickoffOuts += box.pickoffOuts;
    team.buntAttempts += box.buntAttempts;
    team.buntSuccesses += box.buntSuccesses;
    team.infieldFlies += box.infieldFlies;
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
        total.runnerOutsOnBases += team.runnerOutsOnBases;
        total.thrownOutAtHome += team.thrownOutAtHome;
        total.sacFlyAttempts += team.sacFlyAttempts;
        total.sacFlySuccesses += team.sacFlySuccesses;
        total.homeOutfieldAssists += team.homeOutfieldAssists;
        total.errors += team.errors;
        total.badThrows += team.badThrows;
        total.throwingErrors += team.throwingErrors;
        total.stolenBaseAttempts += team.stolenBaseAttempts;
        total.stolenBases += team.stolenBases;
        total.caughtStealing += team.caughtStealing;
        total.wildPitches += team.wildPitches;
        total.passedBalls += team.passedBalls;
        total.balks += team.balks;
        total.pickoffAttempts += team.pickoffAttempts;
        total.pickoffOuts += team.pickoffOuts;
        total.buntAttempts += team.buntAttempts;
        total.buntSuccesses += team.buntSuccesses;
        total.infieldFlies += team.infieldFlies;
    }
    return total;
}

void printLeagueAverages(const TeamAnalysis& league) {
    std::cout << "\n=== League Averages ===\n";
    std::cout << std::left << std::setw(8) << "AVG"
              << std::setw(8) << "OBP"
              << std::setw(8) << "SLG"
              << std::setw(8) << "OPS"
              << std::setw(8) << "BABIP"
              << std::setw(8) << "K%"
              << std::setw(8) << "BB%"
              << std::setw(8) << "HR%"
              << std::setw(8) << "R/G"
              << "\n" << std::string(72, '-') << "\n";
    std::cout << std::left << std::setw(8) << fixed(league.avg(), 3)
              << std::setw(8) << fixed(league.obp(), 3)
              << std::setw(8) << fixed(league.slg(), 3)
              << std::setw(8) << fixed(league.ops(), 3)
              << std::setw(8) << fixed(league.babip(), 3)
              << std::setw(8) << (fixed(league.kPct(), 1) + "%")
              << std::setw(8) << (fixed(league.bbPct(), 1) + "%")
              << std::setw(8) << (fixed(league.hrPct(), 1) + "%")
              << std::setw(8) << fixed(league.runsPerGame(), 2)
              << "\n";
}

void printRunExpectancyReference() {
    std::cout << "\n=== Run Expectancy Reference ===\n";
    std::cout << std::left << std::setw(8) << "Outs"
              << std::setw(8) << "---"
              << std::setw(8) << "1--"
              << std::setw(8) << "-2-"
              << std::setw(8) << "--3"
              << std::setw(8) << "12-"
              << std::setw(8) << "1-3"
              << std::setw(8) << "-23"
              << std::setw(8) << "123"
              << "\n" << std::string(72, '-') << "\n";
    for (int outs = 0; outs < 3; ++outs) {
        std::cout << std::left << std::setw(8) << outs
                  << std::setw(8) << fixed(joji::runExpectancy(outs, false, false, false), 2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, true,  false, false), 2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, false, true,  false), 2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, false, false, true),  2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, true,  true,  false), 2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, true,  false, true),  2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, false, true,  true),  2)
                  << std::setw(8) << fixed(joji::runExpectancy(outs, true,  true,  true),  2)
                  << "\n";
    }
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
              << std::setw(7) << "OOB/G"
              << std::setw(7) << "TOH/G"
              << std::setw(7) << "SF%"
              << std::setw(8) << "H-OFA/G"
              << "\n" << std::string(108, '-') << "\n";

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
                  << std::setw(7) << fixed(team.oobPerGame(), 3)
                  << std::setw(7) << fixed(team.tohPerGame(), 3)
                  << std::setw(7) << (fixed(team.sfPct(), 1) + "%")
                  << std::setw(8) << fixed(team.hOfaPerGame(), 3)
                  << "\n";
    }
}

void printCalibration(const TeamAnalysis& league) {
    std::cout << "\n=== Calibration Metrics ===\n";
    std::cout << std::left << std::setw(9) << "E/G"
              << std::setw(9) << "BadTh/G"
              << std::setw(9) << "ThE/G"
              << std::setw(9) << "SBA/G"
              << std::setw(9) << "SB%"
              << std::setw(9) << "CS/G"
              << std::setw(9) << "WP/G"
              << std::setw(9) << "PB/G"
              << std::setw(9) << "Balk/G"
              << std::setw(9) << "POA/G"
              << std::setw(9) << "PO/G"
              << std::setw(9) << "Bunt/G"
              << std::setw(9) << "Bunt%"
              << std::setw(9) << "IFF/G"
              << "\n" << std::string(126, '-') << "\n";
    std::cout << std::left << std::setw(9) << fixed(league.errorsPerGame(), 2)
              << std::setw(9) << fixed(league.badThrowsPerGame(), 2)
              << std::setw(9) << fixed(league.throwingErrorsPerGame(), 2)
              << std::setw(9) << fixed(league.sbAttemptPerGame(), 3)
              << std::setw(9) << (fixed(league.sbPct(), 1) + "%")
              << std::setw(9) << fixed(league.csPerGame(), 3)
              << std::setw(9) << fixed(league.wpPerGame(), 3)
              << std::setw(9) << fixed(league.pbPerGame(), 3)
              << std::setw(9) << fixed(league.balkPerGame(), 3)
              << std::setw(9) << fixed(league.pickoffAttemptPerGame(), 3)
              << std::setw(9) << fixed(league.pickoffOutPerGame(), 3)
              << std::setw(9) << fixed(league.buntAttemptPerGame(), 3)
              << std::setw(9) << (fixed(league.buntSuccessPct(), 1) + "%")
              << std::setw(9) << fixed(league.iffPerGame(), 3)
              << "\n";
}

std::string resultTypeLabel(joji::AtBatResultType type) {
    switch (type) {
        case joji::AtBatResultType::StrikeOut:         return "StrikeOut";
        case joji::AtBatResultType::Walk:              return "Walk";
        case joji::AtBatResultType::HitByPitch:        return "HitByPitch";
        case joji::AtBatResultType::Single:            return "Single";
        case joji::AtBatResultType::Double:            return "Double";
        case joji::AtBatResultType::Triple:            return "Triple";
        case joji::AtBatResultType::HomeRun:           return "HomeRun";
        case joji::AtBatResultType::GroundOut:         return "GroundOut";
        case joji::AtBatResultType::FlyOut:            return "FlyOut";
        case joji::AtBatResultType::Error:             return "Error";
        case joji::AtBatResultType::FielderChoice:     return "FielderChoice";
        case joji::AtBatResultType::InfieldFly:        return "InfieldFly";
        case joji::AtBatResultType::SacrificeBunt:     return "SacrificeBunt";
        case joji::AtBatResultType::BuntSingle:        return "BuntSingle";
        case joji::AtBatResultType::SqueezeBunt:       return "SqueezeBunt";
        case joji::AtBatResultType::BuntFielderChoice: return "BuntFielderChoice";
    }
    return "Unknown";
}

bool replayPlanMatches(const joji::AnimationPlan& plan,
                       const std::string& filter) {
    if (filter == "any") {
        return !plan.replayTimeline.events.empty();
    }
    if (filter == "batted") {
        return plan.hasBattedBall;
    }
    if (filter == "throw") {
        return !plan.throws.empty() || !plan.throwMovements.empty();
    }
    if (filter == "runner") {
        return !plan.runners.empty() || !plan.runnerMovements.empty();
    }
    if (filter == "tag") {
        return !plan.tagPlays.empty();
    }
    if (filter == "defense") {
        return !plan.defenders.empty();
    }
    return false;
}

int runReplayExportMode(int argc, char* argv[]) {
    const std::string outputPath = argc >= 3 ? argv[2] : "../build/replay_debug.json";
    const std::string filter = argc >= 4 ? argv[3] : "throw";
    const int maxPitches = argc >= 5 ? std::max(1, std::atoi(argv[4])) : 5000;
    if (filter != "any"
        && filter != "batted"
        && filter != "throw"
        && filter != "runner"
        && filter != "tag"
        && filter != "defense") {
        std::cerr << "Unknown replay-export filter: " << filter << "\n"
                  << "Usage: JojiAnalysisRunner replay-export [path] "
                  << "[any|batted|throw|runner|tag|defense] [max-pitches]\n";
        return 2;
    }

    const auto teams = joji::allTeams();
    joji::GameEngine engine{
        teams.at(0),
        teams.at(1),
        joji::Random{std::optional<std::uint32_t>{424242u}}
    };

    int pitchCount = 0;
    std::optional<joji::PlayResult> matchedPlay;
    std::optional<joji::AnimationPlan> matchedPlan;

    while (!engine.isComplete() && pitchCount < maxPitches) {
        engine.simulateNextPitch();
        ++pitchCount;

        if (engine.latestAnimationPlan().has_value()
            && replayPlanMatches(*engine.latestAnimationPlan(), filter)) {
            matchedPlan = engine.latestAnimationPlan();
            break;
        }

        if (engine.hasPendingAtBatResult()) {
            auto play = engine.applyPendingAtBatResult();
            if (engine.latestAnimationPlan().has_value()
                && replayPlanMatches(*engine.latestAnimationPlan(), filter)) {
                matchedPlay = play;
                matchedPlan = engine.latestAnimationPlan();
                break;
            }
        }
    }

    if (!matchedPlan.has_value()) {
        std::cerr << "No replay sample matched filter '" << filter
                  << "' within " << maxPitches << " pitches.\n";
        return 1;
    }

    std::ofstream out(outputPath);
    if (!out) {
        std::cerr << "Could not write replay JSON: " << outputPath << "\n";
        return 1;
    }
    out << joji::exportAnimationPlanToJson(*matchedPlan);

    std::cout << "Replay export written: " << outputPath << "\n";
    std::cout << "Filter: " << filter
              << "  pitchesScanned: " << pitchCount
              << "  duration: " << fixed(matchedPlan->totalDurationSeconds, 2)
              << "s"
              << "  events: " << matchedPlan->replayTimeline.events.size()
              << "  runners: " << matchedPlan->runners.size()
              << "  throws: " << matchedPlan->throws.size()
              << "  tags: " << matchedPlan->tagPlays.size()
              << "\n";
    if (matchedPlay.has_value()) {
        std::cout << "Play: " << matchedPlay->batterName
                  << " " << resultTypeLabel(matchedPlay->type);
        if (!matchedPlay->events.empty()) {
            std::cout << "  " << matchedPlay->events.front();
        }
        std::cout << "\n";
    } else {
        std::cout << "Play: inter-pitch event\n";
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "replay-export") {
        return runReplayExportMode(argc, argv);
    }

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

    const TeamAnalysis league = leagueTotals(analysis);
    printLeagueAverages(league);
    printRunExpectancyReference();
    printTeamIdentity(analysis);
    printCalibration(league);

    return 0;
}
