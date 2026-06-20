#include "GameEngine.h"
#include "Teams.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int GAMES_PER_SERIES = 3;

// ── 累積構造体 ──────────────────────────────────────────────────────────────

struct BatterAccum {
    std::string name;
    std::string team;
    int atBats     = 0;
    int hits       = 0;
    int doubles_   = 0;
    int triples    = 0;
    int homeRuns   = 0;
    int walks      = 0;
    int strikeouts = 0;
    int rbi        = 0;
    int totalBases = 0;
    int sacFlies   = 0;

    int    pa()  const { return atBats + walks + sacFlies; }
    double avg() const { return atBats > 0 ? static_cast<double>(hits) / atBats : 0.0; }
    double obp() const {
        const int d = atBats + walks + sacFlies;
        return d > 0 ? static_cast<double>(hits + walks) / d : 0.0;
    }
    double slg() const { return atBats > 0 ? static_cast<double>(totalBases) / atBats : 0.0; }
    double ops() const { return obp() + slg(); }
};

struct PitcherAccum {
    std::string name;
    std::string team;
    int games           = 0;
    int gamesStarted    = 0;
    int wins            = 0;
    int outsRecorded    = 0;
    int runsAllowed     = 0;
    int earnedRuns      = 0;
    int strikeouts      = 0;
    int walks           = 0;
    int homeRunsAllowed = 0;
    int hitsAllowed     = 0;
    int saves           = 0;
    int holds           = 0;
    int blownSaves      = 0;

    double ip()    const { return outsRecorded / 3.0; }
    double era()   const { return outsRecorded > 0 ? earnedRuns * 27.0 / outsRecorded : 0.0; }
    double whip()  const { return outsRecorded > 0 ? (walks + hitsAllowed) * 3.0 / outsRecorded : 0.0; }
    double kPer9() const { return outsRecorded > 0 ? strikeouts * 27.0 / outsRecorded : 0.0; }
};

struct TeamBaserunAccum {
    std::string name;
    int xbt = 0;
    int oob = 0;
    int toh = 0;
    int sfa = 0;
    int sfs = 0;
    int ofa = 0;
    int games = 0;

    double sfPct()   const { return sfa > 0 ? sfs * 100.0 / sfa : 0.0; }
    double xbtPerG() const { return games > 0 ? static_cast<double>(xbt) / games : 0.0; }
    double oobPerG() const { return games > 0 ? static_cast<double>(oob) / games : 0.0; }
    double tohPerG() const { return games > 0 ? static_cast<double>(toh) / games : 0.0; }
    double ofaPerG() const { return games > 0 ? static_cast<double>(ofa) / games : 0.0; }
};

struct TeamAccum {
    std::string name;
    int totalW  = 0;
    int totalL  = 0;
    int totalRS = 0;
    int totalRA = 0;
    int seasons = 0;

    double pct()    const { return (totalW + totalL) ? static_cast<double>(totalW) / (totalW + totalL) : 0.0; }
    double avgW()   const { return seasons ? static_cast<double>(totalW)  / seasons : 0.0; }
    double avgL()   const { return seasons ? static_cast<double>(totalL)  / seasons : 0.0; }
    double rsPerG() const { return (totalW + totalL) ? static_cast<double>(totalRS) / (totalW + totalL) : 0.0; }
    double raPerG() const { return (totalW + totalL) ? static_cast<double>(totalRA) / (totalW + totalL) : 0.0; }
};

// ── タイトルカウント ────────────────────────────────────────────────────────

struct TitleBoard {
    std::map<std::string, int> battingChamp;
    std::map<std::string, int> homeRunKing;
    std::map<std::string, int> rbiKing;
    std::map<std::string, int> eraTitle;
    std::map<std::string, int> winTitle;
    std::map<std::string, int> saveTitle;
    std::map<std::string, int> kTitle;
};

void awardTitles(const std::map<std::string, BatterAccum>& batters,
                 const std::map<std::string, PitcherAccum>& pitchers,
                 TitleBoard& board, int minPA, double minIP)
{
    auto bestB = [&](auto cmp) -> const BatterAccum* {
        const BatterAccum* best = nullptr;
        for (const auto& [k, b] : batters)
            if (!best || cmp(b, *best)) best = &b;
        return best;
    };
    auto bestBFiltered = [&](int pa, auto cmp) -> const BatterAccum* {
        const BatterAccum* best = nullptr;
        for (const auto& [k, b] : batters) {
            if (b.pa() < pa) continue;
            if (!best || cmp(b, *best)) best = &b;
        }
        return best;
    };
    auto bestP = [&](auto cmp) -> const PitcherAccum* {
        const PitcherAccum* best = nullptr;
        for (const auto& [k, p] : pitchers)
            if (!best || cmp(p, *best)) best = &p;
        return best;
    };
    auto bestPFiltered = [&](double ip, auto cmp) -> const PitcherAccum* {
        const PitcherAccum* best = nullptr;
        for (const auto& [k, p] : pitchers) {
            if (p.ip() < ip) continue;
            if (!best || cmp(p, *best)) best = &p;
        }
        return best;
    };

    if (auto* b = bestBFiltered(minPA, [](const BatterAccum& a, const BatterAccum& x){ return a.avg() > x.avg(); }))
        board.battingChamp[b->name]++;
    if (auto* b = bestB([](const BatterAccum& a, const BatterAccum& x){ return a.homeRuns > x.homeRuns; }))
        board.homeRunKing[b->name]++;
    if (auto* b = bestB([](const BatterAccum& a, const BatterAccum& x){ return a.rbi > x.rbi; }))
        board.rbiKing[b->name]++;
    if (auto* p = bestPFiltered(minIP, [](const PitcherAccum& a, const PitcherAccum& x){ return a.era() < x.era(); }))
        board.eraTitle[p->name]++;
    if (auto* p = bestP([](const PitcherAccum& a, const PitcherAccum& x){ return a.wins > x.wins; }))
        board.winTitle[p->name]++;
    if (auto* p = bestP([](const PitcherAccum& a, const PitcherAccum& x){ return a.saves > x.saves; }))
        board.saveTitle[p->name]++;
    if (auto* p = bestP([](const PitcherAccum& a, const PitcherAccum& x){ return a.strikeouts > x.strikeouts; }))
        board.kTitle[p->name]++;
}

// ── シーズン実行 ────────────────────────────────────────────────────────────

void accumBatters(const std::vector<joji::PlayerBoxScore>& stats, const std::string& teamName,
                  std::map<std::string, BatterAccum>& m1, std::map<std::string, BatterAccum>& m2) {
    for (const auto& ps : stats) {
        const std::string key = teamName + "|" + ps.name;
        for (auto* m : {&m1, &m2}) {
            auto& b    = (*m)[key];
            b.name      = ps.name;
            b.team      = teamName;
            b.atBats   += ps.atBats;
            b.hits     += ps.hits;
            b.doubles_ += ps.doubles;
            b.triples  += ps.triples;
            b.homeRuns += ps.homeRuns;
            b.walks    += ps.walks;
            b.strikeouts += ps.strikeouts;
            b.rbi      += ps.rbi;
            b.totalBases += ps.totalBases;
            b.sacFlies += ps.sacFlies;
        }
    }
}

void accumPitchers(const std::vector<joji::PitcherBoxScore>& stats, const std::string& teamName,
                   std::map<std::string, PitcherAccum>& m1, std::map<std::string, PitcherAccum>& m2) {
    for (const auto& ps : stats) {
        const std::string key = teamName + "|" + ps.name;
        for (auto* m : {&m1, &m2}) {
            auto& p         = (*m)[key];
            p.name           = ps.name;
            p.team           = teamName;
            p.games         += ps.games;
            p.gamesStarted  += ps.gamesStarted;
            p.wins          += ps.wins;
            p.outsRecorded  += ps.outsRecorded;
            p.runsAllowed   += ps.runsAllowed;
            p.earnedRuns    += ps.earnedRuns;
            p.strikeouts    += ps.strikeouts;
            p.walks         += ps.walks;
            p.homeRunsAllowed += ps.homeRunsAllowed;
            p.hitsAllowed   += ps.hitsAllowed;
            p.saves         += ps.saves;
            p.holds         += ps.holds;
            p.blownSaves    += ps.blownSaves;
        }
    }
}

void runSeason(const std::vector<joji::Team>& teams,
               std::vector<TeamAccum>& acc,
               std::map<std::string, BatterAccum>&    allBatters,
               std::map<std::string, PitcherAccum>&   allPitchers,
               std::map<std::string, BatterAccum>&    seasonBatters,
               std::map<std::string, PitcherAccum>&   seasonPitchers,
               std::map<std::string, TeamBaserunAccum>& baserunMap,
               uint32_t& seed)
{
    const int n = static_cast<int>(teams.size());
    std::vector<int> sw(n, 0), sl(n, 0), srs(n, 0), sra(n, 0);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int g = 0; g < GAMES_PER_SERIES; ++g) {
                joji::GameEngine engine{
                    teams[i], teams[j],
                    joji::Random{std::optional<uint32_t>{seed++}}
                };
                std::ostringstream sink;
                engine.simulate(sink);

                const auto res = engine.result();
                if (res.type == joji::GameResultType::AwayWin) { sw[i]++; sl[j]++; }
                else if (res.type == joji::GameResultType::HomeWin) { sw[j]++; sl[i]++; }
                srs[i] += res.awayScore; sra[i] += res.homeScore;
                srs[j] += res.homeScore; sra[j] += res.awayScore;

                accumBatters(engine.awayPlayerStats(), teams[i].name(), allBatters, seasonBatters);
                accumBatters(engine.homePlayerStats(), teams[j].name(), allBatters, seasonBatters);
                accumPitchers(engine.awayPitcherStats(), teams[i].name(), allPitchers, seasonPitchers);
                accumPitchers(engine.homePitcherStats(), teams[j].name(), allPitchers, seasonPitchers);

                auto accumBR = [&](const joji::TeamBoxScore& bs,
                                   const std::vector<joji::PlayerBoxScore>& ps,
                                   const std::string& teamName) {
                    auto& br = baserunMap[teamName];
                    br.name = teamName;
                    br.xbt += bs.extraBasesTaken;
                    br.oob += bs.runnerOutsOnBases;
                    br.toh += bs.runnersThrownOutAtHome;
                    br.sfa += bs.sacFlyAttempts;
                    br.sfs += bs.sacFlySuccesses;
                    br.games += 1;
                    for (const auto& p : ps) br.ofa += p.outfieldAssists;
                };
                accumBR(engine.awayBoxScore(), engine.awayPlayerStats(), teams[i].name());
                accumBR(engine.homeBoxScore(), engine.homePlayerStats(), teams[j].name());
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        acc[i].totalW  += sw[i];
        acc[i].totalL  += sl[i];
        acc[i].totalRS += srs[i];
        acc[i].totalRA += sra[i];
        acc[i].seasons++;
    }
}

// ── 表示関数 ────────────────────────────────────────────────────────────────

void printStandings(const std::vector<TeamAccum>& acc) {
    std::vector<TeamAccum> sorted = acc;
    std::sort(sorted.begin(), sorted.end(),
              [](const TeamAccum& a, const TeamAccum& b){ return a.pct() > b.pct(); });

    const double leaderW = sorted.front().avgW();

    std::cout << "\n=== Average Standings ===\n";
    std::cout << std::left  << std::setw(22) << "Team"
              << std::right << std::setw(6)  << "W"
              << std::setw(6)  << "L"
              << std::setw(7)  << "PCT"
              << std::setw(6)  << "GB"
              << std::setw(7)  << "RS/G"
              << std::setw(7)  << "RA/G"
              << "\n"
              << std::string(61, '-') << "\n";

    for (const auto& t : sorted) {
        const double gb = leaderW - t.avgW();
        std::ostringstream pct, rs, ra, gbStr;
        pct   << std::fixed << std::setprecision(3) << t.pct();
        rs    << std::fixed << std::setprecision(2) << t.rsPerG();
        ra    << std::fixed << std::setprecision(2) << t.raPerG();
        gbStr << std::fixed << std::setprecision(1) << gb;

        std::cout << std::left  << std::setw(22) << t.name
                  << std::right
                  << std::setw(6) << std::fixed << std::setprecision(1) << t.avgW()
                  << std::setw(6) << t.avgL()
                  << std::setw(7) << pct.str()
                  << std::setw(6) << (gb < 0.05 ? " —" : gbStr.str())
                  << std::setw(7) << rs.str()
                  << std::setw(7) << ra.str()
                  << "\n";
    }
}

// 打者ランキング (AVG / HR / RBI)
void printBattingLeaders(std::vector<BatterAccum> vec, int minPA, int topN) {
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [minPA](const BatterAccum& b){ return b.pa() < minPA; }), vec.end());

    auto header = [&](const std::string& label) {
        std::cout << "\n--- " << label << " ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Player"
                  << std::setw(5)  << "Tm";
    };

    // AVG / OBP / SLG / OPS
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const BatterAccum& a, const BatterAccum& b){ return a.avg() > b.avg(); });
        header("Batting Average");
        std::cout << std::right
                  << std::setw(8) << "PA"
                  << std::setw(7) << "AVG"
                  << std::setw(7) << "OBP"
                  << std::setw(7) << "SLG"
                  << std::setw(7) << "OPS"
                  << "\n" << std::string(58, '-') << "\n";
        int rk = 1;
        for (const auto& b : s) {
            if (rk > topN) break;
            std::ostringstream avg, obp, slg, ops;
            avg << std::fixed << std::setprecision(3) << b.avg();
            obp << std::fixed << std::setprecision(3) << b.obp();
            slg << std::fixed << std::setprecision(3) << b.slg();
            ops << std::fixed << std::setprecision(3) << b.ops();
            const std::string abbr = b.team.size() >= 3 ? b.team.substr(0, 3) : b.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << b.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << b.pa()
                      << std::setw(7) << avg.str()
                      << std::setw(7) << obp.str()
                      << std::setw(7) << slg.str()
                      << std::setw(7) << ops.str()
                      << "\n";
        }
    }

    // HR
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const BatterAccum& a, const BatterAccum& b){ return a.homeRuns > b.homeRuns; });
        header("Home Runs");
        std::cout << std::right
                  << std::setw(6) << "HR"
                  << std::setw(8) << "RBI"
                  << std::setw(8) << "SLG"
                  << "\n" << std::string(51, '-') << "\n";
        int rk = 1;
        for (const auto& b : s) {
            if (rk > topN) break;
            std::ostringstream slg;
            slg << std::fixed << std::setprecision(3) << b.slg();
            const std::string abbr = b.team.size() >= 3 ? b.team.substr(0, 3) : b.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << b.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(6) << b.homeRuns
                      << std::setw(8) << b.rbi
                      << std::setw(8) << slg.str()
                      << "\n";
        }
    }

    // RBI
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const BatterAccum& a, const BatterAccum& b){ return a.rbi > b.rbi; });
        header("RBI");
        std::cout << std::right
                  << std::setw(8) << "RBI"
                  << std::setw(6) << "HR"
                  << std::setw(8) << "AVG"
                  << "\n" << std::string(51, '-') << "\n";
        int rk = 1;
        for (const auto& b : s) {
            if (rk > topN) break;
            std::ostringstream avg;
            avg << std::fixed << std::setprecision(3) << b.avg();
            const std::string abbr = b.team.size() >= 3 ? b.team.substr(0, 3) : b.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << b.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << b.rbi
                      << std::setw(6) << b.homeRuns
                      << std::setw(8) << avg.str()
                      << "\n";
        }
    }
}

// 投手ランキング (ERA / W / SV / K)
void printPitchingLeaders(std::vector<PitcherAccum> vec, double minIP, int topN) {
    // ERA: IP フィルター
    {
        auto s = vec;
        s.erase(std::remove_if(s.begin(), s.end(),
            [minIP](const PitcherAccum& p){ return p.ip() < minIP; }), s.end());
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.era() < b.era(); });

        std::cout << "\n--- ERA ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(8) << "IP"
                  << std::setw(6) << "W"
                  << std::setw(7) << "ERA"
                  << std::setw(7) << "WHIP"
                  << std::setw(7) << "K/9"
                  << "\n" << std::string(64, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, era, whip, k9;
            ip   << std::fixed << std::setprecision(1) << p.ip();
            era  << std::fixed << std::setprecision(2) << p.era();
            whip << std::fixed << std::setprecision(2) << p.whip();
            k9   << std::fixed << std::setprecision(1) << p.kPer9();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << ip.str()
                      << std::setw(6) << p.wins
                      << std::setw(7) << era.str()
                      << std::setw(7) << whip.str()
                      << std::setw(7) << k9.str()
                      << "\n";
        }
    }

    // Wins (フィルターなし)
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.wins > b.wins; });
        std::cout << "\n--- Wins ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(5) << "W"
                  << std::setw(8) << "IP"
                  << std::setw(7) << "ERA"
                  << "\n" << std::string(49, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, era;
            ip  << std::fixed << std::setprecision(1) << p.ip();
            era << std::fixed << std::setprecision(2) << p.era();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(5) << p.wins
                      << std::setw(8) << ip.str()
                      << std::setw(7) << era.str()
                      << "\n";
        }
    }

    // Saves
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.saves > b.saves; });
        std::cout << "\n--- Saves ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(5) << "SV"
                  << std::setw(5) << "BS"
                  << std::setw(7) << "ERA"
                  << "\n" << std::string(46, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream era;
            era << std::fixed << std::setprecision(2) << p.era();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(5) << p.saves
                      << std::setw(5) << p.blownSaves
                      << std::setw(7) << era.str()
                      << "\n";
        }
    }

    // Strikeouts (IP フィルター)
    {
        auto s = vec;
        s.erase(std::remove_if(s.begin(), s.end(),
            [minIP](const PitcherAccum& p){ return p.ip() < minIP; }), s.end());
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.strikeouts > b.strikeouts; });
        std::cout << "\n--- Strikeouts ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(7) << "K"
                  << std::setw(8) << "IP"
                  << std::setw(7) << "K/9"
                  << "\n" << std::string(51, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, k9;
            ip << std::fixed << std::setprecision(1) << p.ip();
            k9 << std::fixed << std::setprecision(1) << p.kPer9();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(7) << p.strikeouts
                      << std::setw(8) << ip.str()
                      << std::setw(7) << k9.str()
                      << "\n";
        }
    }
}

void printTitleBoard(const TitleBoard& board, int nSeasons) {
    auto topStr = [](const std::map<std::string, int>& counts, int n = 4) {
        std::vector<std::pair<int, std::string>> v;
        for (const auto& [name, cnt] : counts) v.push_back({cnt, name});
        std::sort(v.rbegin(), v.rend());
        std::string r;
        for (int i = 0; i < std::min(n, static_cast<int>(v.size())); ++i) {
            if (i) r += "  /  ";
            r += v[i].second + " x" + std::to_string(v[i].first);
        }
        return r;
    };

    std::cout << "\n=== Title Board (" << nSeasons << " seasons) ===\n"
              << std::string(80, '-') << "\n";
    std::cout << std::left << std::setw(20) << "首位打者"     << topStr(board.battingChamp) << "\n";
    std::cout << std::left << std::setw(20) << "本塁打王"     << topStr(board.homeRunKing)  << "\n";
    std::cout << std::left << std::setw(20) << "打点王"       << topStr(board.rbiKing)      << "\n";
    std::cout << std::left << std::setw(20) << "最優秀防御率" << topStr(board.eraTitle)     << "\n";
    std::cout << std::left << std::setw(20) << "最多勝"       << topStr(board.winTitle)     << "\n";
    std::cout << std::left << std::setw(20) << "最多セーブ"   << topStr(board.saveTitle)    << "\n";
    std::cout << std::left << std::setw(20) << "最多奪三振"   << topStr(board.kTitle)       << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    const int nSeasons = (argc >= 2) ? std::stoi(argv[1]) : 100;

    std::cout << "Joji Baseball Engine v1.0 — Season Runner\n"
              << nSeasons << " seasons  x  45 games  =  " << nSeasons * 45 << " total\n";

    const auto teams = joji::allTeams();
    const int n = static_cast<int>(teams.size());

    std::vector<TeamAccum> acc(n);
    for (int i = 0; i < n; ++i) acc[i].name = teams[i].name();

    std::map<std::string, BatterAccum>      allBatters;
    std::map<std::string, PitcherAccum>     allPitchers;
    std::map<std::string, TeamBaserunAccum> baserunMap;
    TitleBoard titles;

    uint32_t seed = 42;
    // 1シーズンのタイトル基準
    const int    minPAperSeason = 80;   // ~80% of expected ~100 PA/season
    const double minIPperSeason = 25.0; // ~30% of expected ~80 IP/season

    for (int s = 0; s < nSeasons; ++s) {
        std::map<std::string, BatterAccum>  seasonBatters;
        std::map<std::string, PitcherAccum> seasonPitchers;

        runSeason(teams, acc, allBatters, allPitchers, seasonBatters, seasonPitchers, baserunMap, seed);
        awardTitles(seasonBatters, seasonPitchers, titles, minPAperSeason, minIPperSeason);

        if ((s + 1) % 10 == 0)
            std::cerr << "  " << (s + 1) << "/" << nSeasons << " done\r";
    }
    std::cerr << "\n";

    // キャリアリーダーの最低基準: nSeasons の半分以上出場
    const int    minPAcareer = minPAperSeason * nSeasons / 2;
    const double minIPcareer = minIPperSeason * nSeasons / 2;

    std::vector<BatterAccum>  batterVec;
    for (const auto& [k, b] : allBatters)  batterVec.push_back(b);
    std::vector<PitcherAccum> pitcherVec;
    for (const auto& [k, p] : allPitchers) pitcherVec.push_back(p);

    printStandings(acc);

    std::cout << "\n=== Batting Leaders (min " << minPAcareer << " PA) ===";
    printBattingLeaders(batterVec, minPAcareer, 10);

    std::cout << "\n=== Pitching Leaders (min " << static_cast<int>(minIPcareer) << " IP) ===";
    printPitchingLeaders(pitcherVec, minIPcareer, 10);

    printTitleBoard(titles, nSeasons);

    // ── 走塁 / 外野補殺 ───────────────────────────────────────────────────────
    {
        std::vector<TeamAccum> sorted = acc;
        std::sort(sorted.begin(), sorted.end(),
                  [](const TeamAccum& a, const TeamAccum& b){ return a.pct() > b.pct(); });

        std::vector<TeamBaserunAccum> brVec;
        brVec.reserve(baserunMap.size());
        for (auto& kv : baserunMap) brVec.push_back(kv.second);
        std::sort(brVec.begin(), brVec.end(),
            [&sorted](const TeamBaserunAccum& a, const TeamBaserunAccum& b) {
                auto rank = [&sorted](const std::string& n) {
                    for (std::size_t i = 0; i < sorted.size(); ++i)
                        if (sorted[i].name == n) return i;
                    return sorted.size();
                };
                return rank(a.name) < rank(b.name);
            });

        std::cout << "\n=== Baserunning / Home Outfield Assist (per game avg) ===\n";
        std::cout << std::left  << std::setw(22) << "Team"
                  << std::right << std::setw(7)  << "XBT/G"
                  << std::setw(7)  << "TOH/G"
                  << std::setw(7)  << "SF%"
                  << std::setw(8)  << "H-OFA/G"
                  << "\n" << std::string(49, '-') << "\n";

        for (const auto& t : brVec) {
            std::ostringstream xbt, toh, sfp, ofa;
            xbt << std::fixed << std::setprecision(2) << t.xbtPerG();
            toh << std::fixed << std::setprecision(3) << t.tohPerG();
            sfp << std::fixed << std::setprecision(1) << t.sfPct() << "%";
            ofa << std::fixed << std::setprecision(3) << t.ofaPerG();
            std::cout << std::left  << std::setw(22) << t.name
                      << std::right << std::setw(7)  << xbt.str()
                      << std::setw(7)  << toh.str()
                      << std::setw(7)  << sfp.str()
                      << std::setw(8)  << ofa.str()
                      << "\n";
        }
    }

    return 0;
}
