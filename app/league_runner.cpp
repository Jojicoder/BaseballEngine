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

// ────────── Team batting/record accum ──────────
struct TeamAccum {
    std::string name;
    int totalW  = 0;
    int totalL  = 0;
    int totalRS = 0;
    int totalRA = 0;
    int seasons = 0;

    double avgW()    const { return seasons ? static_cast<double>(totalW)  / seasons : 0.0; }
    double avgL()    const { return seasons ? static_cast<double>(totalL)  / seasons : 0.0; }
    double pct()     const { return (totalW + totalL) ? static_cast<double>(totalW) / (totalW + totalL) : 0.0; }
    double rsPerG()  const { return (totalW + totalL) ? static_cast<double>(totalRS) / (totalW + totalL) : 0.0; }
    double raPerG()  const { return (totalW + totalL) ? static_cast<double>(totalRA) / (totalW + totalL) : 0.0; }
    double diffPerG()const { return rsPerG() - raPerG(); }
};

// ────────── Pitcher accum ──────────
struct PitcherAccum {
    std::string name;
    std::string team;
    int games              = 0;
    int gamesStarted       = 0;
    int outsRecorded       = 0;
    int runsAllowed        = 0;
    int earnedRuns         = 0;
    int strikeouts         = 0;
    int walks              = 0;
    int homeRunsAllowed    = 0;
    int hitsAllowed        = 0;
    int wins               = 0;
    int saves              = 0;
    int holds              = 0;
    int blownSaves         = 0;
    int gamesFinished      = 0;
    int inheritedRunners   = 0;
    int inheritedRunnersScored = 0;

    double ip()    const { return outsRecorded / 3.0; }
    double era()   const { return outsRecorded > 0 ? earnedRuns      * 27.0 / outsRecorded : 0.0; }
    double whip()  const { return outsRecorded > 0 ? (walks + hitsAllowed) * 3.0 / outsRecorded : 0.0; }
    double kPer9() const { return outsRecorded > 0 ? strikeouts       * 27.0 / outsRecorded : 0.0; }
    double hr9()   const { return outsRecorded > 0 ? homeRunsAllowed  * 27.0 / outsRecorded : 0.0; }
};

// ────────── Team baserunning / home outfield assist accum ──────────
struct TeamBaserunAccum {
    std::string name;
    int xbt = 0;   // extraBasesTaken (offensive)
    int oob = 0;   // runnerOutsOnBases (offensive)
    int toh = 0;   // runnersThrownOutAtHome (offensive)
    int sfa = 0;   // sacFlyAttempts (offensive)
    int sfs = 0;   // sacFlySuccesses (offensive)
    int ofa = 0;   // home outfield assists (defensive)
    int games = 0;

    double sfPct()   const { return sfa > 0 ? sfs * 100.0 / sfa : 0.0; }
    double xbtPerG() const { return games > 0 ? static_cast<double>(xbt) / games : 0.0; }
    double oobPerG() const { return games > 0 ? static_cast<double>(oob) / games : 0.0; }
    double tohPerG() const { return games > 0 ? static_cast<double>(toh) / games : 0.0; }
    double ofaPerG() const { return games > 0 ? static_cast<double>(ofa) / games : 0.0; }
};

// ────────── Team pitching accum (for team ERA table) ──────────
struct TeamPitchAccum {
    std::string name;
    int outsRecorded    = 0;
    int runsAllowed     = 0;
    int earnedRuns      = 0;
    int strikeouts      = 0;
    int walks           = 0;
    int homeRunsAllowed = 0;
    int hitsAllowed     = 0;
    int games           = 0;

    double era()   const { return outsRecorded > 0 ? earnedRuns      * 27.0 / outsRecorded : 0.0; }
    double raPerG()const { return games > 0        ? static_cast<double>(runsAllowed) / games : 0.0; }
    double kPct()  const {
        const int bf = outsRecorded + walks + hitsAllowed; // approx
        return bf > 0 ? static_cast<double>(strikeouts) / bf : 0.0;
    }
    double bbPct() const {
        const int bf = outsRecorded + walks + hitsAllowed;
        return bf > 0 ? static_cast<double>(walks) / bf : 0.0;
    }
    double hr9()   const { return outsRecorded > 0 ? homeRunsAllowed * 27.0 / outsRecorded : 0.0; }
    double whip()  const { return outsRecorded > 0 ? (walks + hitsAllowed) * 3.0 / outsRecorded : 0.0; }
};

// ────────── Printers ──────────

void printStandings1(const std::vector<TeamAccum>& acc) {
    std::cout << std::left  << std::setw(22) << "Team"
              << std::right << std::setw(4)  << "W"
              << std::setw(4)  << "L"
              << std::setw(7)  << "PCT"
              << std::setw(5)  << "RS"
              << std::setw(5)  << "RA"
              << std::setw(6)  << "DIFF"
              << "\n";
    std::cout << std::string(53, '-') << "\n";
    for (const auto& t : acc) {
        std::ostringstream pct;
        pct << std::fixed << std::setprecision(3) << t.pct();
        const int diff = t.totalRS - t.totalRA;
        std::cout << std::left  << std::setw(22) << t.name
                  << std::right << std::setw(4)  << t.totalW
                  << std::setw(4)  << t.totalL
                  << std::setw(7)  << pct.str()
                  << std::setw(5)  << t.totalRS
                  << std::setw(5)  << t.totalRA
                  << std::setw(6)  << (diff >= 0 ? "+" : "") + std::to_string(diff)
                  << "\n";
    }
}

void printStandingsMulti(const std::vector<TeamAccum>& acc, int simCount, int gamesPerTeam) {
    std::cout << "Simulations : " << simCount << "\n";
    std::cout << "Games/team  : " << gamesPerTeam << "\n\n";

    std::cout << std::left  << std::setw(22) << "Team"
              << std::right << std::setw(6)  << "AVG_W"
              << std::setw(6)  << "AVG_L"
              << std::setw(7)  << "PCT"
              << std::setw(6)  << "RS/G"
              << std::setw(6)  << "RA/G"
              << std::setw(8)  << "DIFF/G"
              << "\n";
    std::cout << std::string(61, '-') << "\n";

    for (const auto& t : acc) {
        std::ostringstream pct, rs, ra, diff, w, l;
        pct  << std::fixed << std::setprecision(3) << t.pct();
        rs   << std::fixed << std::setprecision(1) << t.rsPerG();
        ra   << std::fixed << std::setprecision(1) << t.raPerG();
        w    << std::fixed << std::setprecision(1) << t.avgW();
        l    << std::fixed << std::setprecision(1) << t.avgL();
        const double d = t.diffPerG();
        diff << std::fixed << std::setprecision(1) << (d >= 0 ? "+" : "") << d;

        std::cout << std::left  << std::setw(22) << t.name
                  << std::right << std::setw(6)  << w.str()
                  << std::setw(6)  << l.str()
                  << std::setw(7)  << pct.str()
                  << std::setw(6)  << rs.str()
                  << std::setw(6)  << ra.str()
                  << std::setw(8)  << diff.str()
                  << "\n";
    }
}

void printTeamPitching(std::vector<TeamPitchAccum> tp) {
    std::sort(tp.begin(), tp.end(),
              [](const TeamPitchAccum& a, const TeamPitchAccum& b){ return a.era() < b.era(); });

    std::cout << std::left  << std::setw(22) << "Team"
              << std::right << std::setw(6)  << "ERA"
              << std::setw(6)  << "RA/G"
              << std::setw(6)  << "WHIP"
              << std::setw(7)  << "K%"
              << std::setw(7)  << "BB%"
              << std::setw(7)  << "HR/9"
              << "\n";
    std::cout << std::string(59, '-') << "\n";
    for (const auto& t : tp) {
        std::ostringstream era, ra, whip, kp, bb, hr;
        era  << std::fixed << std::setprecision(2) << t.era();
        ra   << std::fixed << std::setprecision(2) << t.raPerG();
        whip << std::fixed << std::setprecision(2) << t.whip();
        kp   << std::fixed << std::setprecision(1) << t.kPct() * 100 << "%";
        bb   << std::fixed << std::setprecision(1) << t.bbPct() * 100 << "%";
        hr   << std::fixed << std::setprecision(2) << t.hr9();
        std::cout << std::left  << std::setw(22) << t.name
                  << std::right << std::setw(6)  << era.str()
                  << std::setw(6)  << ra.str()
                  << std::setw(6)  << whip.str()
                  << std::setw(7)  << kp.str()
                  << std::setw(7)  << bb.str()
                  << std::setw(7)  << hr.str()
                  << "\n";
    }
}

void printPitcherStats(std::vector<PitcherAccum> pitchers, int minIP) {
    // 規定投球回以上のみ
    pitchers.erase(std::remove_if(pitchers.begin(), pitchers.end(),
        [minIP](const PitcherAccum& p){ return static_cast<int>(p.ip()) < minIP; }),
        pitchers.end());

    std::sort(pitchers.begin(), pitchers.end(),
              [](const PitcherAccum& a, const PitcherAccum& b){ return a.era() < b.era(); });

    std::cout << std::left  << std::setw(20) << "Pitcher"
              << std::setw(5)  << "Team"
              << std::right << std::setw(5)  << "G"
              << std::setw(5)  << "GS"
              << std::setw(8)  << "IP"
              << std::setw(6)  << "H"
              << std::setw(6)  << "R"
              << std::setw(6)  << "ER"
              << std::setw(6)  << "BB"
              << std::setw(6)  << "K"
              << std::setw(5)  << "HR"
              << std::setw(5)  << "W"
              << std::setw(6)  << "ERA"
              << std::setw(6)  << "WHIP"
              << std::setw(5)  << "SV"
              << std::setw(5)  << "HLD"
              << std::setw(5)  << "BS"
              << std::setw(5)  << "GF"
              << std::setw(5)  << "IR"
              << std::setw(5)  << "IRS"
              << "\n";
    std::cout << std::string(100, '-') << "\n";
    for (const auto& p : pitchers) {
        std::ostringstream ip, era, whip;
        ip   << std::fixed << std::setprecision(1) << p.ip();
        era  << std::fixed << std::setprecision(2) << p.era();
        whip << std::fixed << std::setprecision(2) << p.whip();
        const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
        std::cout << std::left  << std::setw(20) << p.name
                  << std::setw(5)  << abbr
                  << std::right << std::setw(5)  << p.games
                  << std::setw(5)  << p.gamesStarted
                  << std::setw(8)  << ip.str()
                  << std::setw(6)  << p.hitsAllowed
                  << std::setw(6)  << p.runsAllowed
                  << std::setw(6)  << p.earnedRuns
                  << std::setw(6)  << p.walks
                  << std::setw(6)  << p.strikeouts
                  << std::setw(5)  << p.homeRunsAllowed
                  << std::setw(5)  << p.wins
                  << std::setw(6)  << era.str()
                  << std::setw(6)  << whip.str()
                  << std::setw(5)  << p.saves
                  << std::setw(5)  << p.holds
                  << std::setw(5)  << p.blownSaves
                  << std::setw(5)  << p.gamesFinished
                  << std::setw(5)  << p.inheritedRunners
                  << std::setw(5)  << p.inheritedRunnersScored
                  << "\n";
    }
}

void printBaserunning(std::vector<TeamBaserunAccum> teams,
                      const std::vector<TeamAccum>& standingsOrder) {
    // standings と同じ順に並べる
    std::sort(teams.begin(), teams.end(),
        [&standingsOrder](const TeamBaserunAccum& a, const TeamBaserunAccum& b) {
            auto rank = [&standingsOrder](const std::string& n) {
                for (std::size_t i = 0; i < standingsOrder.size(); ++i)
                    if (standingsOrder[i].name == n) return i;
                return standingsOrder.size();
            };
            return rank(a.name) < rank(b.name);
        });

    std::cout << std::left  << std::setw(22) << "Team"
              << std::right << std::setw(7)  << "XBT/G"
              << std::setw(7)  << "TOH/G"
              << std::setw(7)  << "SF%"
              << std::setw(8)  << "H-OFA/G"
              << "\n"
              << std::string(49, '-') << "\n";

    for (const auto& t : teams) {
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

// ────────── Season runner ──────────

void runSeason(const std::vector<joji::Team>& teams,
               std::vector<TeamAccum>& acc,
               std::map<std::string, PitcherAccum>& pitcherMap,
               std::map<std::string, TeamPitchAccum>& teamPitchMap,
               std::map<std::string, TeamBaserunAccum>& baserunMap,
               uint32_t& seed)
{
    const int n = static_cast<int>(teams.size());

    std::vector<int> sw(static_cast<std::size_t>(n), 0);
    std::vector<int> sl(static_cast<std::size_t>(n), 0);
    std::vector<int> srs(static_cast<std::size_t>(n), 0);
    std::vector<int> sra(static_cast<std::size_t>(n), 0);

    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int g = 0; g < GAMES_PER_SERIES; ++g) {
                const int awayIdx = (g % 2 == 0) ? i : j;
                const int homeIdx = (g % 2 == 0) ? j : i;

                joji::GameEngine engine{teams[awayIdx], teams[homeIdx], joji::Random{seed++}};
                std::ostringstream sink;
                engine.simulate(sink);

                const auto& state = engine.state();
                const bool awayWon = state.awayScore > state.homeScore;

                sw[awayWon ? awayIdx : homeIdx] += 1;
                sl[awayWon ? homeIdx : awayIdx] += 1;
                srs[awayIdx] += state.awayScore;
                sra[awayIdx] += state.homeScore;
                srs[homeIdx] += state.homeScore;
                sra[homeIdx] += state.awayScore;

                // 投手成績を蓄積
                auto accumPitchers = [&](const std::vector<joji::PitcherBoxScore>& pstats,
                                         const std::string& teamName) {
                    auto& tp = teamPitchMap[teamName];
                    tp.name = teamName;
                    tp.games += 1;
                    for (const auto& ps : pstats) {
                        const std::string key = teamName + "|" + ps.name;
                        auto& pa = pitcherMap[key];
                        pa.name             = ps.name;
                        pa.team             = teamName;
                        pa.games            += ps.games;
                        pa.gamesStarted     += ps.gamesStarted;
                        pa.outsRecorded     += ps.outsRecorded;
                        pa.runsAllowed      += ps.runsAllowed;
                        pa.earnedRuns       += ps.earnedRuns;
                        pa.strikeouts       += ps.strikeouts;
                        pa.walks            += ps.walks;
                        pa.homeRunsAllowed       += ps.homeRunsAllowed;
                        pa.hitsAllowed           += ps.hitsAllowed;
                        pa.wins                  += ps.wins;
                        pa.saves                 += ps.saves;
                        pa.holds                 += ps.holds;
                        pa.blownSaves            += ps.blownSaves;
                        pa.gamesFinished         += ps.gamesFinished;
                        pa.inheritedRunners      += ps.inheritedRunners;
                        pa.inheritedRunnersScored+= ps.inheritedRunnersScored;

                        tp.outsRecorded     += ps.outsRecorded;
                        tp.runsAllowed      += ps.runsAllowed;
                        tp.earnedRuns       += ps.earnedRuns;
                        tp.strikeouts       += ps.strikeouts;
                        tp.walks            += ps.walks;
                        tp.homeRunsAllowed  += ps.homeRunsAllowed;
                        tp.hitsAllowed      += ps.hitsAllowed;
                    }
                };

                accumPitchers(engine.awayPitcherStats(), teams[awayIdx].name());
                accumPitchers(engine.homePitcherStats(), teams[homeIdx].name());

                // 走塁指標の蓄積
                auto accumBaserun = [&](const joji::TeamBoxScore& bs,
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
                accumBaserun(engine.awayBoxScore(), engine.awayPlayerStats(), teams[awayIdx].name());
                accumBaserun(engine.homeBoxScore(), engine.homePlayerStats(), teams[homeIdx].name());
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        acc[si].totalW  += sw[si];
        acc[si].totalL  += sl[si];
        acc[si].totalRS += srs[si];
        acc[si].totalRA += sra[si];
        acc[si].seasons += 1;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    int simCount = 1;
    if (argc >= 2) {
        simCount = std::stoi(argv[1]);
        if (simCount < 1) simCount = 1;
    }

    const std::vector<joji::Team> teams = joji::allTeams();
    const int n = static_cast<int>(teams.size());
    const int gamesPerTeam   = (n - 1) * GAMES_PER_SERIES;
    const int gamesPerSeason = n * (n - 1) / 2 * GAMES_PER_SERIES;

    std::vector<TeamAccum> acc;
    acc.reserve(static_cast<std::size_t>(n));
    for (const auto& t : teams) acc.push_back({t.name()});

    std::map<std::string, PitcherAccum>     pitcherMap;
    std::map<std::string, TeamPitchAccum>   teamPitchMap;
    std::map<std::string, TeamBaserunAccum> baserunMap;

    std::cout << "Joji Baseball Engine v1.0  |  6-Team League\n";
    std::cout << simCount << " simulation" << (simCount > 1 ? "s" : "")
              << "  ×  " << gamesPerSeason << " games = "
              << simCount * gamesPerSeason << " total\n";

    uint32_t seed = 1;
    for (int s = 0; s < simCount; ++s) {
        runSeason(teams, acc, pitcherMap, teamPitchMap, baserunMap, seed);
        if (simCount == 1 || (s + 1) % 10 == 0)
            std::cerr << "  season " << (s + 1) << "/" << simCount << "\r";
    }
    std::cerr << "\n";

    // PCT 降順ソート
    std::sort(acc.begin(), acc.end(), [](const TeamAccum& a, const TeamAccum& b) {
        if (a.pct() != b.pct()) return a.pct() > b.pct();
        return a.diffPerG() > b.diffPerG();
    });

    // ────── Standings ──────
    if (simCount == 1) {
        std::cout << "\n=== Final Standings ===\n";
        printStandings1(acc);
    } else {
        std::cout << "\n=== Average Standings (" << simCount << " seasons) ===\n";
        printStandingsMulti(acc, simCount, gamesPerTeam);
    }

    // ────── Team Pitching ──────
    std::vector<TeamPitchAccum> teamPitchVec;
    teamPitchVec.reserve(teamPitchMap.size());
    for (auto& kv : teamPitchMap) teamPitchVec.push_back(kv.second);

    // Standings と同じ順に並べる
    std::sort(teamPitchVec.begin(), teamPitchVec.end(),
        [&acc](const TeamPitchAccum& a, const TeamPitchAccum& b) {
            auto rank = [&acc](const std::string& name) {
                for (std::size_t i = 0; i < acc.size(); ++i)
                    if (acc[i].name == name) return i;
                return acc.size();
            };
            return rank(a.name) < rank(b.name);
        });

    std::cout << "\n=== Team Pitching ===\n";
    printTeamPitching(teamPitchVec);

    // ────── Pitcher Individual ──────
    std::vector<PitcherAccum> pitcherVec;
    pitcherVec.reserve(pitcherMap.size());
    for (auto& kv : pitcherMap) pitcherVec.push_back(kv.second);

    // 規定投球回 = gamesPerTeam × simCount × 1.0 (1試合1IP相当) → 緩めに設定
    const int minIP = std::max(1, gamesPerTeam * simCount / 15);
    std::cout << "\n=== Individual Pitching (min " << minIP << " IP) ===\n";
    printPitcherStats(pitcherVec, minIP);

    // ────── Baserunning / Home Outfield Assist ──────
    std::vector<TeamBaserunAccum> baserunVec;
    baserunVec.reserve(baserunMap.size());
    for (auto& kv : baserunMap) baserunVec.push_back(kv.second);

    std::cout << "\n=== Baserunning / Home Outfield Assist ===\n";
    std::cout << "  XBT/G = Extra Bases Taken per game (offensive)\n";
    std::cout << "  TOH/G = Thrown Out at Home per game (offensive)\n";
    std::cout << "  SF%   = Sac Fly success rate\n";
    std::cout << "  H-OFA/G = Home outfield assists per game (defensive)\n\n";
    printBaserunning(baserunVec, acc);

    return 0;
}
