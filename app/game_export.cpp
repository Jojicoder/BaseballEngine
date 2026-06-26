// game_export.cpp — 1試合を pitch-by-pitch JSON で出力
// Usage: ./JojiGameExport [seed]
//   seed: 省略時は今日の日付 (YYYYMMDD) をシードとして使用

#include "BallPhysicsEngine.h"
#include "GameEngine.h"
#include "JsonExporter.h"
#include "Teams.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // シード: 引数があればそれを使い、なければ今日の日付
    uint32_t seed = 0;
    if (argc >= 2) {
        seed = static_cast<uint32_t>(std::stoul(argv[1]));
    } else {
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        seed = static_cast<uint32_t>(
            (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday);
    }

    // 今日の日付文字列
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", tm);

    // チームを seed で選択
    auto teams = joji::allTeams();
    const int n = static_cast<int>(teams.size());
    const int awayIdx = (seed / 13) % n;
    int homeIdx = (awayIdx + (seed % 5) + 1) % n;
    if (homeIdx == awayIdx) homeIdx = (awayIdx + 1) % n;

    joji::Team awayTeam = teams[awayIdx];
    joji::Team homeTeam = teams[homeIdx];

    // ローテーション先頭を seed で散らす
    awayTeam.setStarterSlot(seed % 5);
    homeTeam.setStarterSlot((seed + 2) % 5);

    joji::Random rng{std::optional<uint32_t>{seed}};
    joji::GameEngine engine(awayTeam, homeTeam, rng);
    engine.setFastMode(false);
    joji::BallPhysicsEngine physics;

    // ── イベント収集 ──────────────────────────────────────────────────────

    std::vector<std::string> events;

    auto pushEvent = [&](const std::string& json) {
        events.push_back(json);
    };

    int prevInning = 0;
    bool prevTop = true;

    while (!engine.isComplete()) {
        const auto& st = engine.state();

        // 半イニング開始イベント
        if (st.inning != prevInning || st.isTop != prevTop) {
            std::ostringstream ev;
            ev << "{\"type\":\"half_inning\""
               << ",\"inning\":" << st.inning
               << ",\"isTop\":" << (st.isTop ? "true" : "false")
               << ",\"score\":" << joji::exportGameScoreToJson(st)
               << "}";
            pushEvent(ev.str());
            prevInning = st.inning;
            prevTop = st.isTop;
        }

        auto result = engine.simulateNextPlay();
        if (!result) continue;

        const auto* ab = engine.lastAtBat().has_value() ? &engine.lastAtBat().value() : nullptr;

        // 打席内のピッチログを出力
        if (ab) {
            for (const auto& pl : ab->pitchLogs) {
                std::ostringstream ev;
                ev << std::fixed << std::setprecision(2);
                ev << "{\"type\":\"pitch\""
                   << ",\"inning\":" << st.inning
                   << ",\"isTop\":" << (st.isTop ? "true" : "false")
                   << ",\"pitcher\":" << joji::jsonString(ab->pitcher.name)
                   << ",\"batter\":" << joji::jsonString(ab->batter.name)
                   << ",\"pitchType\":" << joji::jsonString(joji::toString(pl.pitch.pitchType))
                   << ",\"outcome\":" << joji::jsonString(joji::toString(pl.pitchOutcome))
                   << ",\"balls\":" << pl.countBefore.balls
                   << ",\"strikes\":" << pl.countBefore.strikes
                   << ",\"outs\":" << st.outs
                   << ",\"score\":" << joji::exportGameScoreToJson(st)
                   << ",\"bases\":" << joji::exportBasesToJson(st)
                   << ",\"velo\":" << pl.pitch.pitchVelocity
                   << ",\"px\":" << pl.pitch.locationX
                   << ",\"pz\":" << pl.pitch.locationZ
                   << ",\"mx\":" << pl.pitch.movementX
                   << ",\"mz\":" << pl.pitch.movementZ
                   << ",\"batHand\":" << joji::jsonString(
                       ab->batter.battingSide == joji::BattingSide::Left ? "L" : "R")
                   << "}";
                pushEvent(ev.str());
            }
        }

        // 打席結果イベント
        const auto& newSt = engine.state();
        {
            std::ostringstream ev;
            std::string resultStr = joji::toString(result->type);
            ev << std::fixed << std::setprecision(1);
            ev << "{\"type\":\"play\""
               << ",\"inning\":" << st.inning
               << ",\"isTop\":" << (st.isTop ? "true" : "false")
               << ",\"batter\":" << joji::jsonString(result->batterName)
               << ",\"pitcher\":" << joji::jsonString(result->pitcherName)
               << ",\"result\":" << joji::jsonString(resultStr)
               << ",\"outs\":" << newSt.outs
               << ",\"score\":" << joji::exportGameScoreToJson(newSt)
               << ",\"bases\":" << joji::exportBasesToJson(newSt)
               << ",\"runsScored\":" << result->runsScored;

            // 打球データ (インプレーかつ battedBallInput がある場合のみ)
            if (ab && ab->battedBallInput.has_value()) {
                const auto ball = physics.simulate(*ab->battedBallInput);
                ev << ",\"hit\":{"
                   << "\"ev\":" << ball.exitVelocity
                   << ",\"la\":" << ball.launchAngle
                   << ",\"sa\":" << ball.sprayAngle
                   << ",\"traj\":[";
                const std::size_t n = ball.trajectory.size();
                const std::size_t step = std::max(std::size_t{1}, n / 16);
                bool first = true;
                for (std::size_t i = 0; i < n; i += step) {
                    if (!first) ev << ",";
                    first = false;
                    ev << std::setprecision(1)
                       << "[" << ball.trajectory[i].x
                       << "," << ball.trajectory[i].y
                       << "," << ball.trajectory[i].z << "]";
                }
                // 最終点を必ず含める
                if (n > 0 && (n - 1) % step != 0) {
                    ev << ",[" << ball.trajectory[n-1].x
                       << "," << ball.trajectory[n-1].y
                       << "," << ball.trajectory[n-1].z << "]";
                }
                ev << "]}";
            }

            ev << "}";
            pushEvent(ev.str());
        }
    }

    // ── ライン スコア ─────────────────────────────────────────────────────

    const auto& awayLS = engine.awayLineScore();
    const auto& homeLS = engine.homeLineScore();

    // ── JSON 出力 ─────────────────────────────────────────────────────────

    std::cout << "{\n";
    std::cout << "  \"date\":" << joji::jsonString(dateBuf) << ",\n";
    std::cout << "  \"seed\":" << seed << ",\n";
    std::cout << "  \"away\":" << joji::jsonString(awayTeam.name()) << ",\n";
    std::cout << "  \"home\":" << joji::jsonString(homeTeam.name()) << ",\n";

    const auto gr = engine.result();
    std::cout << "  \"finalScore\":{\"away\":" << gr.awayScore
              << ",\"home\":" << gr.homeScore << "},\n";

    // line score
    std::cout << "  \"lineScore\":{\n";
    std::cout << "    \"away\":" << joji::exportNullableIntArrayToJson(awayLS) << ",\n";
    std::cout << "    \"home\":" << joji::exportNullableIntArrayToJson(homeLS) << "\n";
    std::cout << "  },\n";

    // starting lineups
    const auto& awayLU = engine.awayLineup();
    std::vector<std::string> awayLineupNames;
    awayLineupNames.reserve(awayLU.size());
    for (std::size_t i = 0; i < awayLU.size(); ++i) {
        awayLineupNames.push_back(awayLU[i].name);
    }
    std::cout << "  \"awayLineup\":" << joji::exportStringArrayToJson(awayLineupNames) << ",\n";

    const auto& homeLU = engine.homeLineup();
    std::vector<std::string> homeLineupNames;
    homeLineupNames.reserve(homeLU.size());
    for (std::size_t i = 0; i < homeLU.size(); ++i) {
        homeLineupNames.push_back(homeLU[i].name);
    }
    std::cout << "  \"homeLineup\":" << joji::exportStringArrayToJson(homeLineupNames) << ",\n";

    // events
    std::cout << "  \"events\":[\n";
    for (std::size_t i = 0; i < events.size(); ++i) {
        std::cout << "    " << events[i];
        if (i + 1 < events.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ],\n";

    // box scores + win/loss/save pitcher (via exportGameToJson fields)
    const auto& awayPS = engine.awayPlayerStats();
    const auto& homePS = engine.homePlayerStats();
    const auto& awayPit = engine.awayPitcherStats();
    const auto& homePit = engine.homePitcherStats();

    // helper: emit pitcher array inline
    auto emitPitcherArray = [&](const std::vector<joji::PitcherBoxScore>& v) {
        std::cout << "[\n";
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& p = v[i];
            double ip = p.outsRecorded / 3.0;
            double era = ip > 0 ? p.earnedRuns * 9.0 / ip : 0.0;
            std::cout << "    {\"name\":" << joji::jsonString(p.name)
                      << ",\"wins\":"    << p.wins
                      << ",\"losses\":"  << p.losses
                      << ",\"saves\":"   << p.saves
                      << ",\"pitches\":" << p.pitches
                      << ",\"ip\":"      << std::fixed << std::setprecision(1) << ip
                      << ",\"hits\":"    << p.hitsAllowed
                      << ",\"er\":"      << p.earnedRuns
                      << ",\"bb\":"      << p.walks
                      << ",\"k\":"       << p.strikeouts
                      << ",\"era\":"     << std::setprecision(2) << era
                      << "}";
            if (i + 1 < v.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]";
    };

    auto emitPlayerArray = [&](const std::vector<joji::PlayerBoxScore>& v) {
        std::cout << "[\n";
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& p = v[i];
            double avg = p.atBats > 0 ? static_cast<double>(p.hits) / p.atBats : 0.0;
            std::cout << "    {\"name\":" << joji::jsonString(p.name)
                      << ",\"age\":"  << p.age
                      << ",\"ab\":"   << p.atBats
                      << ",\"r\":"    << p.runs
                      << ",\"h\":"    << p.hits
                      << ",\"2b\":"   << p.doubles
                      << ",\"3b\":"   << p.triples
                      << ",\"hr\":"   << p.homeRuns
                      << ",\"rbi\":"  << p.rbi
                      << ",\"bb\":"   << p.walks
                      << ",\"k\":"    << p.strikeouts
                      << ",\"avg\":"  << std::fixed << std::setprecision(3) << avg
                      << "}";
            if (i + 1 < v.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]";
    };

    // win/loss/save pitcher names
    auto findName = [](const std::vector<joji::PitcherBoxScore>& v, auto pred) -> std::string {
        for (const auto& p : v) if (pred(p)) return p.name;
        return "";
    };
    const bool awayWon = gr.awayScore > gr.homeScore;
    const auto& winPit  = awayWon ? awayPit : homePit;
    const auto& lossPit = awayWon ? homePit : awayPit;
    std::string wp = findName(winPit,  [](const joji::PitcherBoxScore& p){ return p.wins   > 0; });
    std::string lp = findName(lossPit, [](const joji::PitcherBoxScore& p){ return p.losses > 0; });
    std::string sp = findName(winPit,  [](const joji::PitcherBoxScore& p){ return p.saves  > 0; });

    std::cout << "  \"winPitcher\":"  << joji::jsonString(wp) << ",\n";
    std::cout << "  \"lossPitcher\":" << joji::jsonString(lp) << ",\n";
    std::cout << "  \"savePitcher\":" << (sp.empty() ? "null" : joji::jsonString(sp)) << ",\n";
    std::cout << "  \"awayPlayerStats\": "; emitPlayerArray(awayPS); std::cout << ",\n";
    std::cout << "  \"homePlayerStats\": "; emitPlayerArray(homePS); std::cout << ",\n";
    std::cout << "  \"awayPitcherStats\": "; emitPitcherArray(awayPit); std::cout << ",\n";
    std::cout << "  \"homePitcherStats\": "; emitPitcherArray(homePit); std::cout << "\n";
    std::cout << "}\n";

    return 0;
}
