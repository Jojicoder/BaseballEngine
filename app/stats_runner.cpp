#include "GameEngine.h"
#include "Teams.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct TeamStats {
    std::string name;
    int games = 0;
    int wins = 0;
    int losses = 0;
    int ties = 0;
    int runsFor = 0;
    int runsAgainst = 0;
    int hits = 0;
    int atBats = 0;
    int walks = 0;
    int totalBases = 0;
    int strikeouts = 0;
};

struct BattedBallStats {
    // BIP count
    int ballsInPlay = 0;

    // BIP type (launch angle based)
    int groundBalls = 0;   // LA < 5
    int lineDrives  = 0;   // 5 ≤ LA < 20
    int flyBalls    = 0;   // 20 ≤ LA < 42
    int popups      = 0;   // LA ≥ 42

    // Outcomes
    int singles  = 0;
    int doubles  = 0;
    int triples  = 0;
    int homeRuns = 0;
    int outs     = 0;
    int gidp     = 0;

    // EV buckets (mph)
    int evWeak     = 0;   // EV < 70
    int evSoft     = 0;   // 70 ≤ EV < 80
    int evMedium   = 0;   // 80 ≤ EV < 95
    int evHardHit  = 0;   // 95 ≤ EV < 98
    int evBarrel   = 0;   // EV ≥ 98 && 8 ≤ LA ≤ 50  (barrel zone)
    double evSum   = 0.0; // for average

    // LA distribution bins (deg)
    int laNeg     = 0;   // LA < 0
    int la00_10   = 0;   // 0–10
    int la10_25   = 0;   // 10–25
    int la25_40   = 0;   // 25–40
    int la40plus  = 0;   // 40+

    // Spray (sprayAngle: negative = oppo, positive = pull for RHH convention)
    int sprayPull   = 0;  // |spray| > 15°, same side as timing early
    int sprayCenter = 0;  // |spray| ≤ 15°
    int sprayOppo   = 0;  // |spray| > 15°, opposite side
};

void addBoxScore(TeamStats& stats, const joji::TeamBoxScore& box) {
    stats.hits += box.hits;
    stats.atBats += box.atBats;
    stats.walks += box.walks;
    stats.totalBases += box.totalBases;
    stats.strikeouts += box.strikeouts;
}

void addBattedBall(BattedBallStats& stats, const joji::PlayResult& play) {
    if (play.battedBall.estimatedDistance <= 0.0) return;

    const double ev = play.battedBall.exitVelocity;
    const double la = play.battedBall.launchAngle;
    const double spray = play.battedBall.sprayAngle;

    stats.ballsInPlay++;
    stats.evSum += ev;

    // ── BIP type ──
    if (la < 5.0)       stats.groundBalls++;
    else if (la < 20.0) stats.lineDrives++;
    else if (la >= 42.0 || play.battedBall.classification == "pop fly") stats.popups++;
    else                stats.flyBalls++;

    // ── Outcomes ──
    switch (play.type) {
        case joji::AtBatResultType::Single:   stats.singles++;   break;
        case joji::AtBatResultType::Double:   stats.doubles++;   break;
        case joji::AtBatResultType::Triple:   stats.triples++;   break;
        case joji::AtBatResultType::HomeRun:  stats.homeRuns++;  break;
        case joji::AtBatResultType::GroundOut:
        case joji::AtBatResultType::FlyOut:   stats.outs++;      break;
        default: break;
    }
    for (const auto& event : play.events) {
        if (event.find("double play") != std::string::npos) { stats.gidp++; break; }
    }

    // ── EV buckets ──
    const bool barrelLA = la >= 8.0 && la <= 50.0;
    if      (ev >= 98.0 && barrelLA)  stats.evBarrel++;
    else if (ev >= 95.0)              stats.evHardHit++;
    else if (ev >= 80.0)              stats.evMedium++;
    else if (ev >= 70.0)              stats.evSoft++;
    else                              stats.evWeak++;

    // ── LA bins ──
    if      (la < 0.0)   stats.laNeg++;
    else if (la < 10.0)  stats.la00_10++;
    else if (la < 25.0)  stats.la10_25++;
    else if (la < 40.0)  stats.la25_40++;
    else                 stats.la40plus++;

    // ── Spray (positive = pull side in engine convention) ──
    if      (spray >  15.0) stats.sprayPull++;
    else if (spray < -15.0) stats.sprayOppo++;
    else                    stats.sprayCenter++;
}

void addPlayerBoxScore(joji::PlayerBoxScore& dest, const joji::PlayerBoxScore& src) {
    dest.position = src.position;
    dest.atBats    += src.atBats;
    dest.hits      += src.hits;
    dest.doubles   += src.doubles;
    dest.triples   += src.triples;
    dest.homeRuns  += src.homeRuns;
    dest.walks     += src.walks;
    dest.strikeouts += src.strikeouts;
    dest.totalBases += src.totalBases;
    dest.rbi       += src.rbi;
    dest.sacFlies  += src.sacFlies;
    dest.gidp      += src.gidp;
    dest.reachedOnError += src.reachedOnError;
    dest.putouts   += src.putouts;
    dest.assists   += src.assists;
    dest.errors    += src.errors;
}

double rate(int num, int den) {
    return den > 0 ? static_cast<double>(num) / den : 0.0;
}

void printTeamStats(const TeamStats& s) {
    const int pa = s.atBats + s.walks;
    const double avg = rate(s.hits, s.atBats);
    const double obp = rate(s.hits + s.walks, pa);
    const double slg = rate(s.totalBases, s.atBats);

    std::cout << std::left << std::setw(18) << s.name
              << std::right
              << std::setw(5) << s.wins << "-"
              << std::left << std::setw(4) << s.losses
              << std::setw(4) << s.ties
              << std::right << std::setw(6) << s.runsFor
              << std::setw(6) << s.runsAgainst
              << "   "
              << std::fixed << std::setprecision(3)
              << avg << "  " << obp << "  " << slg << "  " << (obp + slg)
              << "\n";
}

void printPlayerTable(const std::vector<joji::PlayerBoxScore>& players,
                      const std::string& teamName, int games) {
    std::cout << "\n" << teamName << " (per " << games << " games)\n";
    std::cout << std::left << std::setw(18) << "Player"
              << std::right
              << std::setw(6) << "AB" << std::setw(6) << "H"
              << std::setw(5) << "2B" << std::setw(5) << "3B" << std::setw(5) << "HR"
              << std::setw(5) << "BB" << std::setw(5) << "K"
              << std::setw(5) << "RBI" << std::setw(4) << "SF"
              << std::setw(5) << "GDP"
              << std::setw(7) << "AVG" << std::setw(7) << "OBP"
              << std::setw(7) << "SLG" << std::setw(7) << "OPS" << "\n";
    std::cout << std::string(106, '-') << "\n";

    for (const auto& p : players) {
        const int pa = p.atBats + p.walks + p.sacFlies;
        const double avg = rate(p.hits, p.atBats);
        const double obp = rate(p.hits + p.walks, pa);
        const double slg = rate(p.totalBases, p.atBats);
        const double ops = obp + slg;

        std::cout << std::left << std::setw(18) << p.name
                  << std::right
                  << std::setw(6) << p.atBats << std::setw(6) << p.hits
                  << std::setw(5) << p.doubles << std::setw(5) << p.triples
                  << std::setw(5) << p.homeRuns << std::setw(5) << p.walks
                  << std::setw(5) << p.strikeouts << std::setw(5) << p.rbi
                  << std::setw(4) << p.sacFlies << std::setw(5) << p.gidp
                  << std::fixed << std::setprecision(3)
                  << std::setw(7) << avg << std::setw(7) << obp
                  << std::setw(7) << slg << std::setw(7) << ops << "\n";
    }
}

std::string positionLabel(joji::Position position) {
    switch (position) {
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

void printDefenseTable(const std::vector<joji::PlayerBoxScore>& players,
                       const std::string& teamName, int games) {
    std::cout << "\n" << teamName << " defense (per " << games << " games)\n";
    std::cout << std::left << std::setw(18) << "Player"
              << std::setw(5) << "POS"
              << std::right
              << std::setw(6) << "PO" << std::setw(6) << "A"
              << std::setw(6) << "E" << std::setw(6) << "CH"
              << std::setw(7) << "FLD%" << "\n";
    std::cout << std::string(54, '-') << "\n";

    for (const auto& p : players) {
        const int chances = p.putouts + p.assists + p.errors;
        const double fieldingPct = chances > 0
            ? static_cast<double>(p.putouts + p.assists) / chances
            : 0.0;

        std::cout << std::left << std::setw(18) << p.name
                  << std::setw(5) << positionLabel(p.position)
                  << std::right
                  << std::setw(6) << p.putouts
                  << std::setw(6) << p.assists
                  << std::setw(6) << p.errors
                  << std::setw(6) << chances
                  << std::fixed << std::setprecision(3)
                  << std::setw(7) << fieldingPct << "\n";
    }
}

void printBattedBallStats(const BattedBallStats& s, int games) {
    const int bip = std::max(1, s.ballsInPlay);

    auto pct = [&](int n) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << rate(n, bip) * 100.0 << "%";
        return oss.str();
    };
    auto fmt1 = [](double v, int prec = 1) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(prec) << v;
        return oss.str();
    };

    // ── BIP type ──
    std::cout << "\n--- Batted Ball Profile  (n=" << bip << ") ---\n";
    std::cout << std::left << std::setw(14) << "Type"
              << std::right << std::setw(6) << "Count"
              << std::setw(8) << "%" << "\n";
    std::cout << std::string(30, '-') << "\n";
    auto row = [&](const std::string& label, int n) {
        std::cout << std::left  << std::setw(14) << label
                  << std::right << std::setw(6) << n
                  << std::setw(8) << pct(n) << "\n";
    };
    row("GB  (LA< 5)", s.groundBalls);
    row("LD  (5-20)", s.lineDrives);
    row("FB  (20-42)", s.flyBalls);
    row("PU  (42+)", s.popups);

    // ── Outcomes ──
    std::cout << "\n--- Outcomes / BIP ---\n";
    row("Single", s.singles);
    row("Double", s.doubles);
    row("Triple", s.triples);
    row("HomeRun", s.homeRuns);
    row("Out", s.outs);
    std::cout << std::left  << std::setw(14) << "GIDP/G"
              << std::right << std::setw(6) << s.gidp
              << std::setw(8) << fmt1(rate(s.gidp, std::max(1, games * 2)), 3) << "\n";

    // ── EV distribution ──
    const int totalEV = s.evBarrel + s.evHardHit + s.evMedium + s.evSoft + s.evWeak;
    const double avgEV = totalEV > 0 ? s.evSum / s.ballsInPlay : 0.0;
    std::cout << "\n--- Exit Velocity  (avg " << fmt1(avgEV, 1) << " mph) ---\n";
    std::cout << std::left << std::setw(18) << "Bucket"
              << std::right << std::setw(6) << "Count"
              << std::setw(8) << "%" << "\n";
    std::cout << std::string(34, '-') << "\n";
    auto rowEV = [&](const std::string& label, int n) {
        std::cout << std::left  << std::setw(18) << label
                  << std::right << std::setw(6) << n
                  << std::setw(8) << pct(n) << "\n";
    };
    rowEV("Barrel (≥98+LA)", s.evBarrel);
    rowEV("HardHit (≥95)",   s.evHardHit);
    rowEV("Medium (80-95)",  s.evMedium);
    rowEV("Soft (70-80)",    s.evSoft);
    rowEV("Weak (<70)",      s.evWeak);

    // ── LA distribution ──
    std::cout << "\n--- Launch Angle Distribution ---\n";
    std::cout << std::left << std::setw(14) << "Bin"
              << std::right << std::setw(6) << "Count"
              << std::setw(8) << "%" << "\n";
    std::cout << std::string(30, '-') << "\n";
    row("< 0°",   s.laNeg);
    row("0–10°",  s.la00_10);
    row("10–25°", s.la10_25);
    row("25–40°", s.la25_40);
    row("40°+",   s.la40plus);

    // ── Spray distribution ──
    std::cout << "\n--- Spray Direction ---\n";
    row("Pull  (>15°)",   s.sprayPull);
    row("Center (±15°)", s.sprayCenter);
    row("Oppo  (<-15°)", s.sprayOppo);
}

int parseGames(int argc, char* argv[]) {
    if (argc < 2) return 100;
    try { return std::max(1, std::stoi(argv[1])); }
    catch (const std::exception&) { return 100; }
}

int main(int argc, char* argv[]) {
    const int games = parseGames(argc, argv);

    TeamStats knights;  knights.name = "Newark Knights";
    TeamStats titans;   titans.name  = "Queens Titans";

    // Per-player aggregates, initialized from lineup order.
    const auto teams       = joji::allTeams();
    const auto knightsTeam = teams.at(0);
    const auto titansTeam  = teams.at(1);
    std::vector<joji::PlayerBoxScore> knightsPlayers(knightsTeam.lineup().size());
    std::vector<joji::PlayerBoxScore> titansPlayers(titansTeam.lineup().size());
    for (std::size_t i = 0; i < knightsTeam.lineup().size(); ++i) {
        knightsPlayers[i].name = knightsTeam.lineup()[i].name;
        knightsPlayers[i].position = knightsTeam.lineup()[i].position;
    }
    for (std::size_t i = 0; i < titansTeam.lineup().size(); ++i) {
        titansPlayers[i].name = titansTeam.lineup()[i].name;
        titansPlayers[i].position = titansTeam.lineup()[i].position;
    }

    int extraInningGames = 0;
    int tieGames = 0;
    BattedBallStats battedBalls;

    for (int i = 0; i < games; ++i) {
        joji::GameEngine engine{
            teams.at(0),
            teams.at(1),
            joji::Random{static_cast<std::uint32_t>(1000 + i)},
            joji::GameRules{},
            joji::BallparkConfig::generic()
        };

        std::ostringstream sink;
        while (!engine.isComplete()) {
            const auto play = engine.simulateNextPlay(&sink);
            if (play.has_value()) {
                addBattedBall(battedBalls, *play);
            }
        }

        const joji::GameResult r = engine.result();

        knights.games++;
        titans.games++;
        knights.runsFor    += engine.awayBoxScore().runs;
        knights.runsAgainst += engine.homeBoxScore().runs;
        titans.runsFor     += engine.homeBoxScore().runs;
        titans.runsAgainst += engine.awayBoxScore().runs;
        addBoxScore(knights, engine.awayBoxScore());
        addBoxScore(titans,  engine.homeBoxScore());

        switch (r.type) {
            case joji::GameResultType::AwayWin:
                knights.wins++;  titans.losses++;  break;
            case joji::GameResultType::HomeWin:
                knights.losses++; titans.wins++;   break;
            case joji::GameResultType::Tie:
                knights.ties++;  titans.ties++;  tieGames++;  break;
        }

        if (r.innings > 9) extraInningGames++;

        // Accumulate per-player stats.
        const auto& awayStats = engine.awayPlayerStats();
        const auto& homeStats = engine.homePlayerStats();
        for (std::size_t j = 0; j < awayStats.size(); ++j) {
            addPlayerBoxScore(knightsPlayers[j], awayStats[j]);
        }
        for (std::size_t j = 0; j < homeStats.size(); ++j) {
            addPlayerBoxScore(titansPlayers[j], homeStats[j]);
        }
    }

    // --- Team standings ---
    std::cout << "Joji Baseball Engine v1.0  |  " << games << " games\n\n";
    std::cout << std::left << std::setw(18) << "Team"
              << std::right << std::setw(10) << "W-L"
              << std::left << std::setw(4) << "  T"
              << std::right << std::setw(6) << "RF"
              << std::setw(6) << "RA"
              << "   AVG    OBP    SLG    OPS\n";
    std::cout << std::string(72, '-') << "\n";
    printTeamStats(knights);
    printTeamStats(titans);

    std::cout << "\nExtra-inning games: " << extraInningGames
              << " (" << std::fixed << std::setprecision(1)
              << (100.0 * extraInningGames / games) << "%)\n";
    std::cout << "Tie games:          " << tieGames << "\n";

    // --- Player stats ---
    printPlayerTable(knightsPlayers, "Newark Knights", games);
    printPlayerTable(titansPlayers,  "Queens Titans",  games);
    printDefenseTable(knightsPlayers, "Newark Knights", games);
    printDefenseTable(titansPlayers,  "Queens Titans",  games);

    // --- League averages ---
    const int totalAB = knights.atBats + titans.atBats;
    const int totalH  = knights.hits   + titans.hits;
    const int totalBB = knights.walks  + titans.walks;
    const int totalTB = knights.totalBases + titans.totalBases;
    const int totalK  = knights.strikeouts + titans.strikeouts;
    const int totalPA = totalAB + totalBB;

    std::cout << "\nLeague averages\n";
    std::cout << std::string(40, '-') << "\n";

    auto pr = [](const std::string& label, double v, int prec = 3) {
        std::cout << std::left << std::setw(8) << label
                  << std::right << std::fixed << std::setprecision(prec) << v << "\n";
    };

    pr("AVG",  rate(totalH, totalAB));
    pr("OBP",  rate(totalH + totalBB, totalPA));
    pr("SLG",  rate(totalTB, totalAB));
    pr("OPS",  rate(totalH + totalBB, totalPA) + rate(totalTB, totalAB));
    pr("K%",   rate(totalK, totalPA));
    pr("BB%",  rate(totalBB, totalPA));
    pr("R/G",  rate(knights.runsFor + titans.runsFor, games * 2), 2);
    printBattedBallStats(battedBalls, games);

    std::cout << "\nRaw totals\n";
    std::cout << "R=" << (knights.runsFor + titans.runsFor)
              << " H=" << totalH
              << " AB=" << totalAB
              << " BB=" << totalBB
              << " K=" << totalK
              << " TB=" << totalTB << "\n";

    return 0;
}
