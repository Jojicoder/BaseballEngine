#include "GameEngine.h"
#include "Teams.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int INTRA_SERIES = 12;  // intra-league: 5 opponents × 12 = 60 games
constexpr int INTER_SERIES =  6;  // inter-league: 6 opponents × 6  = 36 games

// ── 累積構造体 ──────────────────────────────────────────────────────────────

struct BatterAccum {
    std::string name;
    std::string team;
    int atBats      = 0;
    int hits        = 0;
    int doubles_    = 0;
    int triples     = 0;
    int homeRuns    = 0;
    int walks       = 0;
    int strikeouts  = 0;
    int hitByPitch  = 0;
    int rbi         = 0;
    int totalBases  = 0;
    int sacFlies    = 0;
    int stolenBases    = 0;
    int caughtStealing = 0;

    int    pa()  const { return atBats + walks + hitByPitch + sacFlies; }
    double avg() const { return atBats > 0 ? static_cast<double>(hits) / atBats : 0.0; }
    double obp() const {
        const int d = atBats + walks + hitByPitch + sacFlies;
        return d > 0 ? static_cast<double>(hits + walks + hitByPitch) / d : 0.0;
    }
    double slg()  const { return atBats > 0 ? static_cast<double>(totalBases) / atBats : 0.0; }
    double ops()  const { return obp() + slg(); }
    double babip() const {
        // (H - HR) / (AB - SO - HR + SF)
        const int bip = atBats - strikeouts - homeRuns + sacFlies;
        return bip > 0 ? static_cast<double>(hits - homeRuns) / bip : 0.0;
    }
    double sbPct() const {
        const int att = stolenBases + caughtStealing;
        return att > 0 ? static_cast<double>(stolenBases) / att : 0.0;
    }
    // wOBA: FanGraphs linear weights (2024 scale)
    double woba() const {
        const int singles = hits - doubles_ - triples - homeRuns;
        const double num  = 0.690*walks + 0.720*hitByPitch + 0.888*singles
                          + 1.271*doubles_ + 1.616*triples + 2.101*homeRuns;
        const int denom   = atBats + walks + hitByPitch + sacFlies;
        return denom > 0 ? num / denom : 0.0;
    }
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

    double ip()        const { return outsRecorded / 3.0; }
    double era()       const { return outsRecorded > 0 ? earnedRuns * 27.0 / outsRecorded : 0.0; }
    double whip()      const { return outsRecorded > 0 ? (walks + hitsAllowed) * 3.0 / outsRecorded : 0.0; }
    double kPer9()     const { return outsRecorded > 0 ? strikeouts * 27.0 / outsRecorded : 0.0; }
    double bb9()       const { return outsRecorded > 0 ? walks * 27.0 / outsRecorded : 0.0; }
    int gamesRelief()  const { return games - gamesStarted; }
    // FIP = (13×HR + 3×BB − 2×K) / IP + constant (3.10 ≈ league-average ERA offset)
    double fip() const {
        if (outsRecorded == 0) return 0.0;
        return (13.0*homeRunsAllowed + 3.0*walks - 2.0*strikeouts) * 3.0 / outsRecorded + 3.10;
    }
    // xFIP: HR regressed 50% toward league average (1.20 HR/9) — no fly ball data needed
    double xfip(double lgHrPer9 = 1.20) const {
        if (outsRecorded == 0) return 0.0;
        const double ipVal      = outsRecorded / 3.0;
        const double actualHrP9 = homeRunsAllowed * 9.0 / ipVal;
        const double expHrP9    = 0.5 * actualHrP9 + 0.5 * lgHrPer9;
        const double expHr      = expHrP9 * ipVal / 9.0;
        return (13.0*expHr + 3.0*walks - 2.0*strikeouts) * 3.0 / outsRecorded + 3.10;
    }
};

struct TeamBaserunAccum {
    std::string name;
    int xbt   = 0;
    int oob   = 0;
    int toh   = 0;
    int sfa   = 0;
    int sfs   = 0;
    int ofa   = 0;
    int games = 0;
    // BABIP / 2B-BIP 用
    int hits       = 0;
    int homeRuns   = 0;
    int atBats     = 0;
    int strikeouts = 0;
    int sacFlies   = 0;
    int doubles_   = 0;
    int walks      = 0;

    double sfPct()    const { return sfa > 0 ? sfs * 100.0 / sfa : 0.0; }
    double xbtPerG()  const { return games > 0 ? static_cast<double>(xbt)  / games : 0.0; }
    double oobPerG()  const { return games > 0 ? static_cast<double>(oob)  / games : 0.0; }
    double tohPerG()  const { return games > 0 ? static_cast<double>(toh)  / games : 0.0; }
    double ofaPerG()  const { return games > 0 ? static_cast<double>(ofa)  / games : 0.0; }
    double babip() const {
        const int bip = atBats - strikeouts - homeRuns + sacFlies;
        return bip > 0 ? static_cast<double>(hits - homeRuns) / bip : 0.0;
    }
    double twoBperBIP() const {
        const int bip = atBats - strikeouts - homeRuns + sacFlies;
        return bip > 0 ? static_cast<double>(doubles_) / bip : 0.0;
    }
    int pa() const { return atBats + walks; }
    double kPct()  const { return pa() > 0 ? strikeouts * 100.0 / pa() : 0.0; }
    double bbPct() const { return pa() > 0 ? walks      * 100.0 / pa() : 0.0; }
    double batAvg() const { return atBats > 0 ? static_cast<double>(hits) / atBats : 0.0; }
};

// ポジション別守備累積
struct PosDefAccum {
    int putouts = 0;
    int assists  = 0;
    int errors   = 0;

    double fpct() const {
        const int total = putouts + assists + errors;
        return total > 0 ? static_cast<double>(putouts + assists) / total : 1.0;
    }
};

// チームごとのポジション別守備累積
// key: Position enum → PosDefAccum
using PosDefMap = std::map<joji::Position, PosDefAccum>;

struct TeamAccum {
    std::string name;
    joji::League league = joji::League::A;
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
    std::map<std::string, int> sbTitle;
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
    if (auto* b = bestB([](const BatterAccum& a, const BatterAccum& x){ return a.stolenBases > x.stolenBases; }))
        board.sbTitle[b->name]++;
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
            auto& b       = (*m)[key];
            b.name         = ps.name;
            b.team         = teamName;
            b.atBats      += ps.atBats;
            b.hits        += ps.hits;
            b.doubles_    += ps.doubles;
            b.triples     += ps.triples;
            b.homeRuns    += ps.homeRuns;
            b.walks       += ps.walks;
            b.strikeouts  += ps.strikeouts;
            b.hitByPitch  += ps.hitByPitch;
            b.rbi         += ps.rbi;
            b.totalBases  += ps.totalBases;
            b.sacFlies    += ps.sacFlies;
            b.stolenBases    += ps.stolenBases;
            b.caughtStealing += ps.caughtStealing;
        }
    }
}

void accumPitchers(const std::vector<joji::PitcherBoxScore>& stats, const std::string& teamName,
                   std::map<std::string, PitcherAccum>& m1, std::map<std::string, PitcherAccum>& m2) {
    for (const auto& ps : stats) {
        const std::string key = teamName + "|" + ps.name;
        for (auto* m : {&m1, &m2}) {
            auto& p          = (*m)[key];
            p.name            = ps.name;
            p.team            = teamName;
            p.games          += ps.games;
            p.gamesStarted   += ps.gamesStarted;
            p.wins           += ps.wins;
            p.outsRecorded   += ps.outsRecorded;
            p.runsAllowed    += ps.runsAllowed;
            p.earnedRuns     += ps.earnedRuns;
            p.strikeouts     += ps.strikeouts;
            p.walks          += ps.walks;
            p.homeRunsAllowed += ps.homeRunsAllowed;
            p.hitsAllowed    += ps.hitsAllowed;
            p.saves          += ps.saves;
            p.holds          += ps.holds;
            p.blownSaves     += ps.blownSaves;
        }
    }
}

void runSeason(std::vector<joji::Team>& teams,
               std::vector<TeamAccum>& acc,
               std::map<std::string, BatterAccum>&    allBatters,
               std::map<std::string, PitcherAccum>&   allPitchers,
               std::map<std::string, BatterAccum>&    seasonBatters,
               std::map<std::string, PitcherAccum>&   seasonPitchers,
               std::map<std::string, TeamBaserunAccum>& baserunMap,
               std::map<std::string, PosDefMap>&       posDefMap,
               uint32_t& seed)
{
    const int n = static_cast<int>(teams.size());
    std::vector<int> sw(n, 0), sl(n, 0), srs(n, 0), sra(n, 0);
    // 5人ローテーション追跡: チームごとに何試合目かをカウント
    std::vector<int> rotSlot(n, 0);

    // 連投疲労追跡: pitcher name → 最後に登板したゲーム番号
    int gameCounter = 0;
    std::map<std::string, int> lastAppearanceGame;

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const int seriesLen = (teams[i].league() == teams[j].league())
                                  ? INTRA_SERIES : INTER_SERIES;
            for (int g = 0; g < seriesLen; ++g) {
                teams[i].setStarterSlot(rotSlot[i]++ % 5);
                teams[j].setStarterSlot(rotSlot[j]++ % 5);

                // 連投疲労マップ: 各チームの投手の休養日数を計算
                // 同一チームの投手が1試合に1回登板する想定 (実際のMLBと同じ)
                auto buildFatigue = [&](const joji::Team& team) {
                    std::map<std::string, int> fm;
                    auto addPitcher = [&](const joji::Player& p) {
                        if (auto it = lastAppearanceGame.find(p.name); it != lastAppearanceGame.end())
                            fm[p.name] = gameCounter - it->second - 1;
                    };
                    addPitcher(team.starter());
                    for (const auto& bp : team.bullpen()) addPitcher(bp);
                    return fm;
                };
                auto fatigueI = buildFatigue(teams[i]);
                auto fatigueJ = buildFatigue(teams[j]);

                joji::GameEngine engine{
                    teams[i], teams[j],
                    joji::Random{std::optional<uint32_t>{seed++}},
                    joji::GameRules{},
                    teams[j].homeBallpark()
                };
                engine.setFatigueMap(fatigueI);  // away team fatigue
                std::ostringstream sink;
                engine.simulate(sink);

                // 登板した投手の最終登板ゲームを更新
                for (const auto& ps : engine.awayPitcherStats())
                    if (ps.outsRecorded > 0) lastAppearanceGame[ps.name] = gameCounter;
                for (const auto& ps : engine.homePitcherStats())
                    if (ps.outsRecorded > 0) lastAppearanceGame[ps.name] = gameCounter;
                ++gameCounter;

                const auto res = engine.result();
                if (res.type == joji::GameResultType::AwayWin) { sw[i]++; sl[j]++; }
                else if (res.type == joji::GameResultType::HomeWin) { sw[j]++; sl[i]++; }
                srs[i] += res.awayScore; sra[i] += res.homeScore;
                srs[j] += res.homeScore; sra[j] += res.awayScore;

                accumBatters(engine.awayPlayerStats(), teams[i].name(), allBatters, seasonBatters);
                accumBatters(engine.homePlayerStats(), teams[j].name(), allBatters, seasonBatters);
                accumPitchers(engine.awayPitcherStats(), teams[i].name(), allPitchers, seasonPitchers);
                accumPitchers(engine.homePitcherStats(), teams[j].name(), allPitchers, seasonPitchers);

                // 走塁 / 外野補殺 + BABIP用チームトータル
                auto accumBR = [&](const joji::TeamBoxScore& bs,
                                   const std::vector<joji::PlayerBoxScore>& ps,
                                   const std::string& teamName) {
                    auto& br = baserunMap[teamName];
                    br.name  = teamName;
                    br.xbt  += bs.extraBasesTaken;
                    br.oob  += bs.runnerOutsOnBases;
                    br.toh  += bs.runnersThrownOutAtHome;
                    br.sfa  += bs.sacFlyAttempts;
                    br.sfs  += bs.sacFlySuccesses;
                    br.games += 1;
                    for (const auto& p : ps) br.ofa += p.outfieldAssists;
                    // BABIP用
                    br.hits       += bs.hits;
                    br.homeRuns   += bs.homeRuns;
                    br.atBats     += bs.atBats;
                    br.strikeouts += bs.strikeouts;
                    br.sacFlies   += bs.sacFlySuccesses;
                    br.doubles_   += bs.doubles_;
                    br.walks      += bs.walks;
                };
                accumBR(engine.awayBoxScore(), engine.awayPlayerStats(), teams[i].name());
                accumBR(engine.homeBoxScore(), engine.homePlayerStats(), teams[j].name());

                // ポジション別守備累積
                auto accumPosDef = [&](const std::vector<joji::PlayerBoxScore>& ps,
                                       const std::string& teamName) {
                    auto& pdm = posDefMap[teamName];
                    for (const auto& p : ps) {
                        auto& pd = pdm[p.position];
                        pd.putouts += p.putouts;
                        pd.assists  += p.assists;
                        pd.errors   += p.errors;
                    }
                };
                accumPosDef(engine.awayPlayerStats(), teams[i].name());
                accumPosDef(engine.homePlayerStats(), teams[j].name());
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

// ── プレーオフ ───────────────────────────────────────────────────────────────

// Returns 0 if teamA wins, 1 if teamB wins
int runPlayoffSeries(const joji::Team& teamA, const joji::Team& teamB,
                     int maxGames, uint32_t& seed) {
    const int needed = maxGames / 2 + 1;
    int winsA = 0, winsB = 0;
    while (winsA < needed && winsB < needed) {
        joji::GameEngine eng{teamA, teamB,
                             joji::Random{std::optional<uint32_t>{seed++}}};
        std::ostringstream sink;
        eng.simulate(sink);
        const auto res = eng.result();
        if (res.type == joji::GameResultType::AwayWin) winsA++;
        else winsB++;
    }
    return (winsA >= needed) ? 0 : 1;
}

// Run one season's playoffs (2-league format):
//   League Championship Series: top 2 per league, best-of-5
//   World Series: league champions, best-of-7
// Returns World Series champion name.
std::string runPlayoffs(const std::vector<joji::Team>& teams,
                        const std::vector<int>& seasonWins,
                        uint32_t& seed) {
    const int n = static_cast<int>(teams.size());

    // Split and rank by league
    std::vector<int> leagueA, leagueB;
    for (int i = 0; i < n; ++i) {
        if (teams[i].league() == joji::League::A) leagueA.push_back(i);
        else                                       leagueB.push_back(i);
    }
    auto byWins = [&seasonWins](int a, int b){ return seasonWins[a] > seasonWins[b]; };
    std::sort(leagueA.begin(), leagueA.end(), byWins);
    std::sort(leagueB.begin(), leagueB.end(), byWins);

    // League Championship Series (best-of-5)
    const int lcsa = (runPlayoffSeries(teams[leagueA[0]], teams[leagueA[1]], 5, seed) == 0)
                     ? leagueA[0] : leagueA[1];
    const int lcsb = (runPlayoffSeries(teams[leagueB[0]], teams[leagueB[1]], 5, seed) == 0)
                     ? leagueB[0] : leagueB[1];

    // World Series (best-of-7)
    const int ws = (runPlayoffSeries(teams[lcsa], teams[lcsb], 7, seed) == 0) ? lcsa : lcsb;
    return teams[ws].name();
}

void printChampions(const std::map<std::string, int>& counts, int nSeasons) {
    std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    std::cout << "\n=== World Series Champions (" << nSeasons << " seasons) ===\n";
    for (const auto& [name, c] : sorted) {
        const int barLen = static_cast<int>(c * 30.0 / nSeasons + 0.5);
        std::cout << std::left  << std::setw(24) << name
                  << std::right << std::setw(4)  << c
                  << "  " << std::string(barLen, '#') << "\n";
    }
}

// ── 表示関数 ────────────────────────────────────────────────────────────────

void printLeagueTable(const std::string& title, std::vector<TeamAccum> rows) {
    std::sort(rows.begin(), rows.end(),
              [](const TeamAccum& a, const TeamAccum& b){ return a.pct() > b.pct(); });
    const double leaderW = rows.front().avgW();

    std::cout << "\n=== " << title << " ===\n";
    std::cout << std::left  << std::setw(26) << "Team"
              << std::right << std::setw(6)  << "W"
              << std::setw(6)  << "L"
              << std::setw(7)  << "PCT"
              << std::setw(6)  << "GB"
              << std::setw(7)  << "RS/G"
              << std::setw(7)  << "RA/G"
              << "\n"
              << std::string(65, '-') << "\n";

    for (const auto& t : rows) {
        const double gb = leaderW - t.avgW();
        std::ostringstream pct, rs, ra, gbStr;
        pct   << std::fixed << std::setprecision(3) << t.pct();
        rs    << std::fixed << std::setprecision(2) << t.rsPerG();
        ra    << std::fixed << std::setprecision(2) << t.raPerG();
        gbStr << std::fixed << std::setprecision(1) << gb;

        std::cout << std::left  << std::setw(26) << t.name
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

void printStandings(const std::vector<TeamAccum>& acc) {
    std::vector<TeamAccum> leagueA, leagueB;
    for (const auto& t : acc) {
        if (t.league == joji::League::A) leagueA.push_back(t);
        else                             leagueB.push_back(t);
    }
    printLeagueTable("League A — New York Metro",         leagueA);
    printLeagueTable("League B — Philadelphia Districts", leagueB);
}

// 打者ランキング (AVG / HR / RBI / SB)
void printBattingLeaders(std::vector<BatterAccum> vec, int minPA, int topN) {
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [minPA](const BatterAccum& b){ return b.pa() < minPA; }), vec.end());

    auto header = [&](const std::string& label) {
        std::cout << "\n--- " << label << " ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Player"
                  << std::setw(5)  << "Tm";
    };

    // AVG / OBP / SLG / OPS / BABIP
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
                  << std::setw(7) << "wOBA"
                  << std::setw(7) << "BABIP"
                  << "\n" << std::string(72, '-') << "\n";
        int rk = 1;
        for (const auto& b : s) {
            if (rk > topN) break;
            std::ostringstream avg, obp, slg, ops, woba, bab;
            avg  << std::fixed << std::setprecision(3) << b.avg();
            obp  << std::fixed << std::setprecision(3) << b.obp();
            slg  << std::fixed << std::setprecision(3) << b.slg();
            ops  << std::fixed << std::setprecision(3) << b.ops();
            woba << std::fixed << std::setprecision(3) << b.woba();
            bab  << std::fixed << std::setprecision(3) << b.babip();
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
                      << std::setw(7) << woba.str()
                      << std::setw(7) << bab.str()
                      << "\n";
        }
    }

    // wOBA
    {
        auto s = vec;
        std::sort(s.begin(), s.end(), [](const BatterAccum& a, const BatterAccum& b){ return a.woba() > b.woba(); });
        header("wOBA");
        std::cout << std::right
                  << std::setw(8) << "PA"
                  << std::setw(7) << "wOBA"
                  << std::setw(7) << "OBP"
                  << std::setw(7) << "SLG"
                  << std::setw(6) << "HR"
                  << std::setw(6) << "BB"
                  << "\n" << std::string(63, '-') << "\n";
        int rk = 1;
        for (const auto& b : s) {
            if (rk > topN) break;
            std::ostringstream woba, obp, slg;
            woba << std::fixed << std::setprecision(3) << b.woba();
            obp  << std::fixed << std::setprecision(3) << b.obp();
            slg  << std::fixed << std::setprecision(3) << b.slg();
            const std::string abbr = b.team.size() >= 3 ? b.team.substr(0, 3) : b.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << b.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << b.pa()
                      << std::setw(7) << woba.str()
                      << std::setw(7) << obp.str()
                      << std::setw(7) << slg.str()
                      << std::setw(6) << b.homeRuns
                      << std::setw(6) << b.walks
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

    // Stolen Bases (フィルターなし)
    {
        auto s = vec;
        // フィルターを外して全員対象
        std::vector<BatterAccum> all = s;
        std::sort(all.begin(), all.end(), [](const BatterAccum& a, const BatterAccum& b){ return a.stolenBases > b.stolenBases; });
        header("Stolen Bases");
        std::cout << std::right
                  << std::setw(6) << "SB"
                  << std::setw(5) << "CS"
                  << std::setw(8) << "SB%"
                  << std::setw(8) << "AVG"
                  << "\n" << std::string(56, '-') << "\n";
        int rk = 1;
        for (const auto& b : all) {
            if (rk > topN) break;
            if (b.stolenBases == 0 && b.caughtStealing == 0) break;
            std::ostringstream sbp, avg;
            sbp << std::fixed << std::setprecision(1) << b.sbPct() * 100.0 << "%";
            avg << std::fixed << std::setprecision(3) << b.avg();
            const std::string abbr = b.team.size() >= 3 ? b.team.substr(0, 3) : b.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << b.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(6) << b.stolenBases
                      << std::setw(5) << b.caughtStealing
                      << std::setw(8) << sbp.str()
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
                  << std::setw(5) << "GS"
                  << std::setw(5) << "GR"
                  << std::setw(8) << "IP"
                  << std::setw(5) << "W"
                  << std::setw(7) << "ERA"
                  << std::setw(7) << "FIP"
                  << std::setw(7) << "xFIP"
                  << std::setw(7) << "WHIP"
                  << std::setw(7) << "K/9"
                  << std::setw(7) << "BB/9"
                  << "\n" << std::string(94, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, era, fip, xfip, whip, k9, bb9;
            ip   << std::fixed << std::setprecision(1) << p.ip();
            era  << std::fixed << std::setprecision(2) << p.era();
            fip  << std::fixed << std::setprecision(2) << p.fip();
            xfip << std::fixed << std::setprecision(2) << p.xfip();
            whip << std::fixed << std::setprecision(2) << p.whip();
            k9   << std::fixed << std::setprecision(1) << p.kPer9();
            bb9  << std::fixed << std::setprecision(1) << p.bb9();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(5) << p.gamesStarted
                      << std::setw(5) << p.gamesRelief()
                      << std::setw(8) << ip.str()
                      << std::setw(5) << p.wins
                      << std::setw(7) << era.str()
                      << std::setw(7) << fip.str()
                      << std::setw(7) << xfip.str()
                      << std::setw(7) << whip.str()
                      << std::setw(7) << k9.str()
                      << std::setw(7) << bb9.str()
                      << "\n";
        }
    }

    // FIP (IP フィルター)
    {
        auto s = vec;
        s.erase(std::remove_if(s.begin(), s.end(),
            [minIP](const PitcherAccum& p){ return p.ip() < minIP; }), s.end());
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.fip() < b.fip(); });
        std::cout << "\n--- FIP ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(8) << "IP"
                  << std::setw(7) << "FIP"
                  << std::setw(7) << "xFIP"
                  << std::setw(7) << "ERA"
                  << std::setw(7) << "K/9"
                  << std::setw(5) << "HR"
                  << "\n" << std::string(68, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, fip, xfip, era;
            ip   << std::fixed << std::setprecision(1) << p.ip();
            fip  << std::fixed << std::setprecision(2) << p.fip();
            xfip << std::fixed << std::setprecision(2) << p.xfip();
            era  << std::fixed << std::setprecision(2) << p.era();
            std::ostringstream k9;
            k9   << std::fixed << std::setprecision(1) << p.kPer9();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << ip.str()
                      << std::setw(7) << fip.str()
                      << std::setw(7) << xfip.str()
                      << std::setw(7) << era.str()
                      << std::setw(7) << k9.str()
                      << std::setw(5) << p.homeRunsAllowed
                      << "\n";
        }
    }

    // Wins
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

    // WHIP (IP フィルター)
    {
        auto s = vec;
        s.erase(std::remove_if(s.begin(), s.end(),
            [minIP](const PitcherAccum& p){ return p.ip() < minIP; }), s.end());
        std::sort(s.begin(), s.end(), [](const PitcherAccum& a, const PitcherAccum& b){ return a.whip() < b.whip(); });
        std::cout << "\n--- WHIP ---\n"
                  << std::left  << std::setw(4)  << "Rk"
                  << std::setw(20) << "Pitcher"
                  << std::setw(5)  << "Tm"
                  << std::right
                  << std::setw(8) << "IP"
                  << std::setw(7) << "WHIP"
                  << std::setw(7) << "BB/9"
                  << std::setw(7) << "K/9"
                  << "\n" << std::string(58, '-') << "\n";
        int rk = 1;
        for (const auto& p : s) {
            if (rk > topN) break;
            std::ostringstream ip, whip, bb9, k9;
            ip   << std::fixed << std::setprecision(1) << p.ip();
            whip << std::fixed << std::setprecision(2) << p.whip();
            bb9  << std::fixed << std::setprecision(1) << p.bb9();
            k9   << std::fixed << std::setprecision(1) << p.kPer9();
            const std::string abbr = p.team.size() >= 3 ? p.team.substr(0, 3) : p.team;
            std::cout << std::left  << std::setw(4) << rk++
                      << std::setw(20) << p.name
                      << std::setw(5)  << abbr
                      << std::right
                      << std::setw(8) << ip.str()
                      << std::setw(7) << whip.str()
                      << std::setw(7) << bb9.str()
                      << std::setw(7) << k9.str()
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
    std::cout << std::left << std::setw(20) << "盗塁王"       << topStr(board.sbTitle)      << "\n";
    std::cout << std::left << std::setw(20) << "最優秀防御率" << topStr(board.eraTitle)     << "\n";
    std::cout << std::left << std::setw(20) << "最多勝"       << topStr(board.winTitle)     << "\n";
    std::cout << std::left << std::setw(20) << "最多セーブ"   << topStr(board.saveTitle)    << "\n";
    std::cout << std::left << std::setw(20) << "最多奪三振"   << topStr(board.kTitle)       << "\n";
}

const char* posAbbr(joji::Position pos) {
    switch (pos) {
        case joji::Position::Pitcher:     return "P";
        case joji::Position::Catcher:     return "C";
        case joji::Position::FirstBase:   return "1B";
        case joji::Position::SecondBase:  return "2B";
        case joji::Position::ThirdBase:   return "3B";
        case joji::Position::Shortstop:   return "SS";
        case joji::Position::LeftField:   return "LF";
        case joji::Position::CenterField: return "CF";
        case joji::Position::RightField:  return "RF";
    }
    return "?";
}

// ポジション表示順
const joji::Position kPosOrder[] = {
    joji::Position::Catcher,
    joji::Position::FirstBase,
    joji::Position::SecondBase,
    joji::Position::ThirdBase,
    joji::Position::Shortstop,
    joji::Position::LeftField,
    joji::Position::CenterField,
    joji::Position::RightField,
    joji::Position::Pitcher,
};

void printPositionDefense(const std::map<std::string, PosDefMap>& posDefMap,
                          const std::vector<TeamAccum>& acc) {
    // 順位順に並べたチームリスト
    std::vector<std::string> teamOrder;
    {
        std::vector<TeamAccum> sorted = acc;
        std::sort(sorted.begin(), sorted.end(),
                  [](const TeamAccum& a, const TeamAccum& b){ return a.pct() > b.pct(); });
        for (const auto& t : sorted) teamOrder.push_back(t.name);
    }

    std::cout << "\n=== Position Defense (PO-A-E, FPCT) ===\n";

    for (const auto& teamName : teamOrder) {
        auto it = posDefMap.find(teamName);
        if (it == posDefMap.end()) continue;
        const PosDefMap& pdm = it->second;

        std::cout << "\n" << teamName << "\n"
                  << std::left  << std::setw(5)  << "Pos"
                  << std::right << std::setw(8)  << "PO"
                  << std::setw(8)  << "A"
                  << std::setw(8)  << "E"
                  << std::setw(8)  << "FPCT"
                  << "\n" << std::string(37, '-') << "\n";

        int totPO = 0, totA = 0, totE = 0;
        for (joji::Position pos : kPosOrder) {
            auto pit = pdm.find(pos);
            if (pit == pdm.end()) continue;
            const PosDefAccum& pd = pit->second;
            std::ostringstream fp;
            fp << std::fixed << std::setprecision(3) << pd.fpct();
            std::cout << std::left  << std::setw(5)  << posAbbr(pos)
                      << std::right << std::setw(8)  << pd.putouts
                      << std::setw(8)  << pd.assists
                      << std::setw(8)  << pd.errors
                      << std::setw(8)  << fp.str()
                      << "\n";
            totPO += pd.putouts; totA += pd.assists; totE += pd.errors;
        }
        const double totFpct = (totPO + totA + totE) > 0
            ? static_cast<double>(totPO + totA) / (totPO + totA + totE) : 1.0;
        std::ostringstream tfp;
        tfp << std::fixed << std::setprecision(3) << totFpct;
        std::cout << std::string(37, '-') << "\n"
                  << std::left  << std::setw(5)  << "TOT"
                  << std::right << std::setw(8)  << totPO
                  << std::setw(8)  << totA
                  << std::setw(8)  << totE
                  << std::setw(8)  << tfp.str()
                  << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const int nSeasons = (argc >= 2) ? std::stoi(argv[1]) : 100;

    std::cout << "Joji Baseball Engine — 2-League Season Runner\n"
              << "12 teams  |  96 games/team  |  " << nSeasons << " seasons\n"
              << "League A: New York Metro   |  League B: Philadelphia Districts\n\n";

    auto teams = joji::allTeams();
    const int n = static_cast<int>(teams.size());

    std::vector<TeamAccum> acc(n);
    for (int i = 0; i < n; ++i) {
        acc[i].name   = teams[i].name();
        acc[i].league = teams[i].league();
    }

    std::map<std::string, BatterAccum>      allBatters;
    std::map<std::string, PitcherAccum>     allPitchers;
    std::map<std::string, TeamBaserunAccum> baserunMap;
    std::map<std::string, PosDefMap>        posDefMap;
    TitleBoard titles;
    std::map<std::string, int> championCount;

    uint32_t seed = 42;
    const int    minPAperSeason = 250;  // ~2.6 PA/game × 96 games
    const double minIPperSeason = 60.0; // ~starter gets ~115 IP/season

    for (int s = 0; s < nSeasons; ++s) {
        std::map<std::string, BatterAccum>  seasonBatters;
        std::map<std::string, PitcherAccum> seasonPitchers;

        // Snapshot wins before season for playoff seeding
        std::vector<int> winsSnapshot(n);
        for (int i = 0; i < n; ++i) winsSnapshot[i] = acc[i].totalW;

        runSeason(teams, acc, allBatters, allPitchers,
                  seasonBatters, seasonPitchers,
                  baserunMap, posDefMap, seed);
        awardTitles(seasonBatters, seasonPitchers, titles, minPAperSeason, minIPperSeason);

        // Per-season wins for playoff seeding
        if (n >= 4) {
            std::vector<int> seasonWins(n);
            for (int i = 0; i < n; ++i) seasonWins[i] = acc[i].totalW - winsSnapshot[i];
            const std::string champ = runPlayoffs(teams, seasonWins, seed);
            championCount[champ]++;
        }

        if ((s + 1) % 10 == 0)
            std::cerr << "  " << (s + 1) << "/" << nSeasons << " done\r";
    }
    std::cerr << "\n";

    const int    minPAcareer = minPAperSeason * nSeasons / 2;
    const double minIPcareer = minIPperSeason * nSeasons / 2;

    std::vector<BatterAccum>  batterVec;
    for (const auto& [k, b] : allBatters)  batterVec.push_back(b);
    std::vector<PitcherAccum> pitcherVec;
    for (const auto& [k, p] : allPitchers) pitcherVec.push_back(p);

    printStandings(acc);
    printChampions(championCount, nSeasons);

    std::cout << "\n=== Batting Leaders (min " << minPAcareer << " PA) ===";
    printBattingLeaders(batterVec, minPAcareer, 10);

    std::cout << "\n=== Pitching Leaders (min " << static_cast<int>(minIPcareer) << " IP) ===";
    printPitchingLeaders(pitcherVec, minIPcareer, 10);

    printTitleBoard(titles, nSeasons);

    // ── 走塁 / 外野補殺 / BABIP / 2B-BIP ────────────────────────────────────
    {
        std::vector<TeamAccum> sorted = acc;
        std::sort(sorted.begin(), sorted.end(),
                  [](const TeamAccum& a, const TeamAccum& b){ return a.pct() > b.pct(); });

        std::vector<TeamBaserunAccum> brVec;
        brVec.reserve(baserunMap.size());
        for (auto& kv : baserunMap) brVec.push_back(kv.second);
        std::sort(brVec.begin(), brVec.end(),
            [&sorted](const TeamBaserunAccum& a, const TeamBaserunAccum& b) {
                auto rank = [&sorted](const std::string& nm) {
                    for (std::size_t i = 0; i < sorted.size(); ++i)
                        if (sorted[i].name == nm) return i;
                    return sorted.size();
                };
                return rank(a.name) < rank(b.name);
            });

        std::cout << "\n=== Baserunning / BABIP / 2B-BIP (per game avg) ===\n";
        std::cout << std::left  << std::setw(22) << "Team"
                  << std::right << std::setw(7)  << "XBT/G"
                  << std::setw(7)  << "TOH/G"
                  << std::setw(7)  << "SF%"
                  << std::setw(8)  << "H-OFA/G"
                  << std::setw(8)  << "BABIP"
                  << std::setw(8)  << "2B/BIP"
                  << "\n" << std::string(67, '-') << "\n";

        for (const auto& t : brVec) {
            std::ostringstream xbt, toh, sfp, ofa, bab, twob;
            xbt  << std::fixed << std::setprecision(2) << t.xbtPerG();
            toh  << std::fixed << std::setprecision(3) << t.tohPerG();
            sfp  << std::fixed << std::setprecision(1) << t.sfPct() << "%";
            ofa  << std::fixed << std::setprecision(3) << t.ofaPerG();
            bab  << std::fixed << std::setprecision(3) << t.babip();
            twob << std::fixed << std::setprecision(3) << t.twoBperBIP();
            std::cout << std::left  << std::setw(22) << t.name
                      << std::right << std::setw(7)  << xbt.str()
                      << std::setw(7)  << toh.str()
                      << std::setw(7)  << sfp.str()
                      << std::setw(8)  << ofa.str()
                      << std::setw(8)  << bab.str()
                      << std::setw(8)  << twob.str()
                      << "\n";
        }

        // ── リーグ打撃サマリー (K% / BB% / BA / BABIP) ──────────────────────────
        // Sorted by standings order so it's easy to cross-reference with standings.
        std::cout << "\n=== League Batting Summary ===\n";
        std::cout << std::left  << std::setw(22) << "Team"
                  << std::right << std::setw(7)  << "K%"
                  << std::setw(7)  << "BB%"
                  << std::setw(7)  << "BA"
                  << std::setw(8)  << "BABIP"
                  << std::setw(8)  << "ERA"
                  << "\n" << std::string(59, '-') << "\n";
        for (const auto& t : brVec) {
            // ERA approximated as RA/G (from TeamAccum)
            const double era = [&]() -> double {
                for (const auto& a : acc) {
                    if (a.name == t.name) return a.raPerG();
                }
                return 0.0;
            }();
            std::ostringstream kp, bbp, ba, bab, er;
            kp  << std::fixed << std::setprecision(1) << t.kPct()  << "%";
            bbp << std::fixed << std::setprecision(1) << t.bbPct() << "%";
            ba  << std::fixed << std::setprecision(3) << t.batAvg();
            bab << std::fixed << std::setprecision(3) << t.babip();
            er  << std::fixed << std::setprecision(2) << era;
            std::cout << std::left  << std::setw(22) << t.name
                      << std::right << std::setw(7)  << kp.str()
                      << std::setw(7)  << bbp.str()
                      << std::setw(7)  << ba.str()
                      << std::setw(8)  << bab.str()
                      << std::setw(8)  << er.str()
                      << "\n";
        }
        // League averages
        {
            int totK = 0, totBB = 0, totPA = 0;
            double totBIP = 0.0, totHR = 0.0;
            int totAB = 0, totHits = 0;
            for (const auto& t : brVec) {
                totK  += t.strikeouts; totBB += t.walks; totPA += t.pa();
                totAB += t.atBats;     totHits += t.hits; totHR += t.homeRuns;
                totBIP += t.atBats - t.strikeouts - t.homeRuns + t.sacFlies;
            }
            const double lgK   = totPA  > 0 ? totK  * 100.0 / totPA  : 0.0;
            const double lgBB  = totPA  > 0 ? totBB * 100.0 / totPA  : 0.0;
            const double lgBA  = totAB  > 0 ? static_cast<double>(totHits) / totAB : 0.0;
            const double lgBABIP = totBIP > 0 ? (totHits - totHR) / totBIP : 0.0;
            std::cout << std::string(59, '-') << "\n";
            std::ostringstream kp, bbp, ba, bab;
            kp  << std::fixed << std::setprecision(1) << lgK  << "%";
            bbp << std::fixed << std::setprecision(1) << lgBB << "%";
            ba  << std::fixed << std::setprecision(3) << lgBA;
            bab << std::fixed << std::setprecision(3) << lgBABIP;
            std::cout << std::left  << std::setw(22) << "League Average"
                      << std::right << std::setw(7)  << kp.str()
                      << std::setw(7)  << bbp.str()
                      << std::setw(7)  << ba.str()
                      << std::setw(8)  << bab.str()
                      << std::setw(8)  << "--"
                      << "\n";
        }
    }

    // ── ポジション別守備指標 ────────────────────────────────────────────────
    printPositionDefense(posDefMap, acc);

    return 0;
}
