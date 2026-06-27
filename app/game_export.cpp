// game_export.cpp — one game or a full JBL season as pitch-by-pitch JSON.
//
// Usage:
//   ./JojiGameExport [seed]
//   ./JojiGameExport --season 2026 --all-games --out-dir public/jbl/seasons/2026 [--limit N]

#include "BallPhysicsEngine.h"
#include "GameEngine.h"
#include "JsonExporter.h"
#include "Teams.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kIntraSeries = 12;
constexpr int kInterSeries = 6;

struct CliOptions {
    bool seasonMode = false;
    bool allGames = false;
    int season = 2026;
    int limit = 0;
    std::string outDir;
    std::optional<uint32_t> seed;
};

struct GameExport {
    std::string gameId;
    std::string date;
    int awayIdx = 0;
    int homeIdx = 0;
    std::string away;
    std::string home;
    std::string venue;
    int awayScore = 0;
    int homeScore = 0;
    std::string gameFile;
    std::string json;
};

struct TeamStanding {
    int wins = 0;
    int losses = 0;
    int rs = 0;
    int ra = 0;
};

std::string todayDate() {
    const std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", tm);
    return dateBuf;
}

std::string dateForGame(int season, int gameNumber) {
    std::tm tm{};
    tm.tm_year = season - 1900;
    tm.tm_mon = 3;
    tm.tm_mday = 1 + gameNumber / 6;
    std::mktime(&tm);
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm);
    return dateBuf;
}

std::string halfName(bool isTop) {
    return isTop ? "top" : "bottom";
}

std::string baseName(int base) {
    switch (base) {
        case 0: return "home";
        case 1: return "first";
        case 2: return "second";
        case 3: return "third";
        case 4: return "home";
        default: return "home";
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
    return "CF";
}

std::string scoreJson(const joji::GameState& st) {
    std::ostringstream out;
    out << "{\"away\":" << st.awayScore << ",\"home\":" << st.homeScore << "}";
    return out.str();
}

std::string basesJson(const joji::GameState& st) {
    auto runner = [](const std::optional<std::string>& value) {
        return value ? joji::jsonString(*value) : std::string("null");
    };
    std::ostringstream out;
    out << "{\"first\":" << runner(st.bases[0])
        << ",\"second\":" << runner(st.bases[1])
        << ",\"third\":" << runner(st.bases[2]) << "}";
    return out.str();
}

std::string nullableLineScore(const std::vector<int>& values) {
    return joji::exportNullableIntArrayToJson(values);
}

std::string runnerNamesScored(const joji::GameState& before,
                              const joji::GameState& after,
                              const joji::PlayResult& result) {
    std::vector<std::string> scored;
    int needed = std::max(0, (after.awayScore + after.homeScore) -
                              (before.awayScore + before.homeScore));
    auto presentAfter = [&](const std::string& name) {
        for (const auto& base : after.bases)
            if (base && *base == name) return true;
        return false;
    };
    auto outOnPlay = [&](const std::string& name) {
        for (const auto& runnerOut : result.runnerOuts)
            if (runnerOut.runnerName == name) return true;
        return false;
    };
    for (const auto& base : before.bases) {
        if (needed == 0) break;
        if (base && !presentAfter(*base) && !outOnPlay(*base)) {
            scored.push_back(*base);
            --needed;
        }
    }
    if (needed > 0 && result.type == joji::AtBatResultType::HomeRun) {
        scored.push_back(result.batterName);
    }

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < scored.size(); ++i) {
        if (i > 0) out << ",";
        out << joji::jsonString(scored[i]);
    }
    out << "]";
    return out.str();
}

std::string runnerAdvancesJson(const joji::GameState& before,
                               const joji::GameState& after,
                               const joji::PlayResult& result) {
    std::vector<std::string> items;
    auto afterBaseFor = [&](const std::string& runner) -> int {
        for (int i = 0; i < 3; ++i)
            if (after.bases[i] && *after.bases[i] == runner) return i + 1;
        return -1;
    };
    auto add = [&](const std::string& runner, int from, int to, const std::string& status) {
        std::ostringstream item;
        item << "{\"runner\":" << joji::jsonString(runner)
             << ",\"from\":" << joji::jsonString(baseName(from))
             << ",\"to\":" << joji::jsonString(baseName(to))
             << ",\"result\":" << joji::jsonString(status) << "}";
        items.push_back(item.str());
    };

    for (int i = 0; i < 3; ++i) {
        if (!before.bases[i]) continue;
        const std::string& runner = *before.bases[i];
        bool wasOut = false;
        int outBase = 0;
        for (const auto& runnerOut : result.runnerOuts) {
            if (runnerOut.runnerName == runner) {
                wasOut = true;
                outBase = runnerOut.outAtBase;
                break;
            }
        }
        if (wasOut) {
            add(runner, i + 1, outBase, "out");
            continue;
        }
        const int afterBase = afterBaseFor(runner);
        if (afterBase > 0) add(runner, i + 1, afterBase, afterBase == i + 1 ? "held" : "safe");
        else add(runner, i + 1, 4, "scored");
    }

    const int batterBase = afterBaseFor(result.batterName);
    bool batterAlreadyOut = false;
    for (const auto& runnerOut : result.runnerOuts) {
        if (runnerOut.runnerName == result.batterName) {
            batterAlreadyOut = true;
            break;
        }
    }
    if (batterBase > 0) add(result.batterName, 0, batterBase, "safe");
    else if (!batterAlreadyOut) {
        const bool batterOut =
            result.type == joji::AtBatResultType::StrikeOut ||
            result.type == joji::AtBatResultType::GroundOut ||
            result.type == joji::AtBatResultType::FlyOut ||
            result.type == joji::AtBatResultType::InfieldFly ||
            result.type == joji::AtBatResultType::SacrificeBunt ||
            result.type == joji::AtBatResultType::SqueezeBunt;
        if (batterOut) {
            const int target = result.throwDecision.targetBase > 0
                ? result.throwDecision.targetBase
                : (result.type == joji::AtBatResultType::FlyOut ||
                   result.type == joji::AtBatResultType::InfieldFly ? 0 : 1);
            add(result.batterName, 0, target, "out");
        }
    }
    for (const auto& runnerOut : result.runnerOuts) {
        if (runnerOut.runnerName == result.batterName) {
            add(result.batterName, 0, runnerOut.outAtBase, "out");
        }
    }

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out << ",";
        out << items[i];
    }
    out << "]";
    return out.str();
}

std::string fieldersInvolvedJson(const joji::PlayResult& result) {
    std::vector<std::string> names;
    auto add = [&](const std::string& name) {
        if (!name.empty() &&
            std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    };
    add(result.fielderName);
    add(result.throwDecision.cutoffFielderName);
    add(result.throwingErrorFielderName);

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out << ",";
        out << joji::jsonString(names[i]);
    }
    out << "]";
    return out.str();
}

std::string outRunnersJson(const joji::PlayResult& result) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < result.runnerOuts.size(); ++i) {
        if (i > 0) out << ",";
        out << joji::jsonString(result.runnerOuts[i].runnerName);
    }
    out << "]";
    return out.str();
}

std::string lineupJson(const std::vector<joji::Player>& lineup) {
    std::vector<std::string> names;
    names.reserve(lineup.size());
    for (const auto& player : lineup) names.push_back(player.name);
    return joji::exportStringArrayToJson(names);
}

std::string battersJson(const std::vector<joji::PlayerBoxScore>& stats,
                        const std::string& teamName) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < stats.size(); ++i) {
        const auto& p = stats[i];
        if (i > 0) out << ",";
        out << "{\"name\":" << joji::jsonString(p.name)
            << ",\"team\":" << joji::jsonString(teamName)
            << ",\"age\":" << p.age
            << ",\"position\":" << joji::jsonString(positionLabel(p.position))
            << ",\"ab\":" << p.atBats
            << ",\"r\":" << p.runs
            << ",\"h\":" << p.hits
            << ",\"rbi\":" << p.rbi
            << ",\"bb\":" << p.walks
            << ",\"so\":" << p.strikeouts
            << ",\"hr\":" << p.homeRuns
            << ",\"runs\":" << p.runs
            << "}";
    }
    out << "]";
    return out.str();
}

std::string pitchersJson(const std::vector<joji::PitcherBoxScore>& stats,
                         const std::string& teamName) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < stats.size(); ++i) {
        const auto& p = stats[i];
        if (i > 0) out << ",";
        out << std::fixed << std::setprecision(1)
            << "{\"name\":" << joji::jsonString(p.name)
            << ",\"team\":" << joji::jsonString(teamName)
            << ",\"ip\":" << p.ip()
            << ",\"h\":" << p.hitsAllowed
            << ",\"r\":" << p.runsAllowed
            << ",\"er\":" << p.earnedRuns
            << ",\"bb\":" << p.walks
            << ",\"so\":" << p.strikeouts
            << ",\"hr\":" << p.homeRunsAllowed
            << ",\"pitches\":" << p.pitches
            << ",\"wins\":" << p.wins
            << ",\"losses\":" << p.losses
            << ",\"saves\":" << p.saves
            << "}";
    }
    out << "]";
    return out.str();
}

std::string pitcherWithStat(const std::vector<joji::PitcherBoxScore>& awayStats,
                            const std::vector<joji::PitcherBoxScore>& homeStats,
                            const std::string& stat) {
    auto find = [&](const std::vector<joji::PitcherBoxScore>& stats) -> std::optional<std::string> {
        for (const auto& p : stats) {
            if (stat == "win" && p.wins > 0) return p.name;
            if (stat == "loss" && p.losses > 0) return p.name;
            if (stat == "save" && p.saves > 0) return p.name;
        }
        return std::nullopt;
    };
    if (auto name = find(awayStats)) return joji::jsonString(*name);
    if (auto name = find(homeStats)) return joji::jsonString(*name);
    return "null";
}

std::string teamSlug(const std::string& name) {
    std::string slug;
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            slug.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!slug.empty() && slug.back() != '-') {
            slug.push_back('-');
        }
    }
    if (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug;
}

GameExport simulateGame(const std::vector<joji::Team>& baseTeams,
                        int awayIdx,
                        int homeIdx,
                        int season,
                        uint32_t seed,
                        int awayStarterSlot,
                        int homeStarterSlot,
                        const std::string& date,
                        const std::string& gameId) {
    joji::Team awayTeam = baseTeams[static_cast<std::size_t>(awayIdx)];
    joji::Team homeTeam = baseTeams[static_cast<std::size_t>(homeIdx)];
    awayTeam.setStarterSlot(awayStarterSlot);
    homeTeam.setStarterSlot(homeStarterSlot);

    joji::GameEngine engine(awayTeam, homeTeam, joji::Random{std::optional<uint32_t>{seed}},
                            joji::GameRules{}, homeTeam.homeBallpark());
    engine.setFastMode(false);
    joji::BallPhysicsEngine physics;

    std::vector<std::string> events;
    auto pushEvent = [&](const std::string& json) { events.push_back(json); };

    int prevInning = -1;
    bool prevTop = true;

    while (!engine.isComplete()) {
        const joji::GameState before = engine.state();

        if (before.inning != prevInning || before.isTop != prevTop) {
            std::ostringstream ev;
            ev << "{\"type\":\"half_inning\""
               << ",\"inning\":" << before.inning
               << ",\"half\":" << joji::jsonString(halfName(before.isTop))
               << ",\"isTop\":" << (before.isTop ? "true" : "false")
               << ",\"score\":" << scoreJson(before)
               << "}";
            pushEvent(ev.str());
            prevInning = before.inning;
            prevTop = before.isTop;
        }

        auto result = engine.simulateNextPlay();
        if (!result) continue;

        const joji::GameState after = engine.state();
        const auto* ab = engine.lastAtBat() ? &engine.lastAtBat().value() : nullptr;

        if (ab) {
            for (const auto& pl : ab->pitchLogs) {
                std::ostringstream ev;
                ev << std::fixed << std::setprecision(2);
                ev << "{\"type\":\"pitch\""
                   << ",\"inning\":" << before.inning
                   << ",\"half\":" << joji::jsonString(halfName(before.isTop))
                   << ",\"isTop\":" << (before.isTop ? "true" : "false")
                   << ",\"pitcher\":" << joji::jsonString(ab->pitcher.name)
                   << ",\"batter\":" << joji::jsonString(ab->batter.name)
                   << ",\"pitchType\":" << joji::jsonString(joji::toString(pl.pitch.pitchType))
                   << ",\"outcome\":" << joji::jsonString(joji::toString(pl.pitchOutcome))
                   << ",\"ballsBefore\":" << pl.countBefore.balls
                   << ",\"strikesBefore\":" << pl.countBefore.strikes
                   << ",\"ballsAfter\":" << pl.countAfter.balls
                   << ",\"strikesAfter\":" << pl.countAfter.strikes
                   << ",\"balls\":" << pl.countBefore.balls
                   << ",\"strikes\":" << pl.countBefore.strikes
                   << ",\"outsBefore\":" << before.outs
                   << ",\"outsAfter\":" << after.outs
                   << ",\"outs\":" << before.outs
                   << ",\"scoreBefore\":" << scoreJson(before)
                   << ",\"scoreAfter\":" << scoreJson(after)
                   << ",\"score\":" << scoreJson(before)
                   << ",\"basesBefore\":" << basesJson(before)
                   << ",\"basesAfter\":" << basesJson(after)
                   << ",\"bases\":" << basesJson(before)
                   << ",\"velo\":" << pl.pitch.pitchVelocity
                   << ",\"px\":" << pl.pitch.locationX
                   << ",\"pz\":" << pl.pitch.locationZ
                   << ",\"mx\":" << pl.pitch.movementX
                   << ",\"mz\":" << pl.pitch.movementZ
                   << ",\"batHand\":" << joji::jsonString(
                       ab->batter.battingSide == joji::BattingSide::Left ? "L" : "R")
                   << ",\"pitchHand\":" << joji::jsonString(
                       ab->pitcher.throwingHand == joji::ThrowingHand::Left ? "L" : "R")
                   << ",\"swing\":{\"offered\":"
                   << (pl.swingDecision.decision == joji::SwingDecisionType::Swing ? "true" : "false")
                   << ",\"probability\":" << pl.swingDecision.swingProbability
                   << ",\"reason\":" << joji::jsonString(pl.swingDecision.reason);
                if (pl.swing) {
                    ev << ",\"timingError\":" << pl.swing->timingError
                       << ",\"quality\":" << pl.swing->swingQuality
                       << ",\"batSpeed\":" << pl.swing->batSpeed;
                }
                if (pl.contactResult) {
                    ev << ",\"contact\":{\"barrelAccuracy\":" << pl.contactResult->barrelAccuracy
                       << ",\"timingQuality\":" << pl.contactResult->timingQuality
                       << ",\"verticalError\":" << pl.contactResult->verticalBatError
                       << ",\"horizontalError\":" << pl.contactResult->horizontalBatError
                       << "}";
                }
                ev << "}}";
                pushEvent(ev.str());
            }
        }

        std::ostringstream ev;
        const std::string resultStr = joji::toString(result->type);
        ev << std::fixed << std::setprecision(1);
        ev << "{\"type\":\"play\""
           << ",\"inning\":" << before.inning
           << ",\"half\":" << joji::jsonString(halfName(before.isTop))
           << ",\"isTop\":" << (before.isTop ? "true" : "false")
           << ",\"batter\":" << joji::jsonString(result->batterName)
           << ",\"pitcher\":" << joji::jsonString(result->pitcherName)
           << ",\"result\":" << joji::jsonString(resultStr)
           << ",\"outsBefore\":" << before.outs
           << ",\"outsAfter\":" << after.outs
           << ",\"outs\":" << after.outs
           << ",\"scoreBefore\":" << scoreJson(before)
           << ",\"scoreAfter\":" << scoreJson(after)
           << ",\"score\":" << scoreJson(after)
           << ",\"basesBefore\":" << basesJson(before)
           << ",\"basesAfter\":" << basesJson(after)
           << ",\"bases\":" << basesJson(after)
           << ",\"runsScored\":" << runnerNamesScored(before, after, *result)
           << ",\"rbi\":" << result->runsScored
           << ",\"runnerAdvances\":" << runnerAdvancesJson(before, after, *result)
           << ",\"fieldersInvolved\":" << fieldersInvolvedJson(*result)
           << ",\"outRunners\":" << outRunnersJson(*result);
        if (!result->fielderName.empty()) {
            ev << ",\"fielder\":" << joji::jsonString(result->fielderName);
        }
        if (result->throwDecision.targetBase > 0) {
            ev << ",\"throwTo\":" << joji::jsonString(baseName(result->throwDecision.targetBase));
        }

        if (ab && ab->battedBallInput) {
            const auto ball = physics.simulate(*ab->battedBallInput);
            ev << ",\"hit\":{"
               << "\"ev\":" << ball.exitVelocity
               << ",\"la\":" << ball.launchAngle
               << ",\"sa\":" << ball.sprayAngle
               << ",\"landing\":{\"x\":" << ball.landingPoint.x
               << ",\"y\":" << ball.landingPoint.y << "}"
               << ",\"traj\":[";
            const std::size_t n = ball.trajectory.size();
            const std::size_t step = std::max(std::size_t{1}, n / 16);
            bool first = true;
            for (std::size_t i = 0; i < n; i += step) {
                if (!first) ev << ",";
                first = false;
                ev << "[" << ball.trajectory[i].x
                   << "," << ball.trajectory[i].y
                   << "," << ball.trajectory[i].z << "]";
            }
            if (n > 0 && (n - 1) % step != 0) {
                ev << ",[" << ball.trajectory[n - 1].x
                   << "," << ball.trajectory[n - 1].y
                   << "," << ball.trajectory[n - 1].z << "]";
            }
            ev << "]}";
        }
        ev << "}";
        pushEvent(ev.str());
    }

    const auto gr = engine.result();
    GameExport exported;
    exported.gameId = gameId;
    exported.date = date;
    exported.awayIdx = awayIdx;
    exported.homeIdx = homeIdx;
    exported.away = awayTeam.name();
    exported.home = homeTeam.name();
    exported.venue = homeTeam.homeBallpark().name;
    exported.awayScore = gr.awayScore;
    exported.homeScore = gr.homeScore;
    exported.gameFile = "games/" + gameId + ".json";

    std::ostringstream out;
    out << "{\n";
    out << "  \"schemaVersion\":1,\n";
    out << "  \"gameId\":" << joji::jsonString(gameId) << ",\n";
    out << "  \"season\":" << season << ",\n";
    out << "  \"date\":" << joji::jsonString(date) << ",\n";
    out << "  \"seed\":" << seed << ",\n";
    out << "  \"venue\":" << joji::jsonString(exported.venue) << ",\n";
    out << "  \"away\":" << joji::jsonString(exported.away) << ",\n";
    out << "  \"home\":" << joji::jsonString(exported.home) << ",\n";
    out << "  \"finalScore\":{\"away\":" << gr.awayScore
        << ",\"home\":" << gr.homeScore << "},\n";
    out << "  \"lineScore\":{\n";
    out << "    \"away\":" << nullableLineScore(engine.awayLineScore()) << ",\n";
    out << "    \"home\":" << nullableLineScore(engine.homeLineScore()) << "\n";
    out << "  },\n";
    out << "  \"awayLineup\":" << lineupJson(engine.awayLineup()) << ",\n";
    out << "  \"homeLineup\":" << lineupJson(engine.homeLineup()) << ",\n";
    out << "  \"winPitcher\":" << pitcherWithStat(engine.awayPitcherStats(), engine.homePitcherStats(), "win") << ",\n";
    out << "  \"lossPitcher\":" << pitcherWithStat(engine.awayPitcherStats(), engine.homePitcherStats(), "loss") << ",\n";
    out << "  \"savePitcher\":" << pitcherWithStat(engine.awayPitcherStats(), engine.homePitcherStats(), "save") << ",\n";
    out << "  \"boxScore\":{\n";
    out << "    \"batters\":";
    out << "[";
    const std::string awayBatters = battersJson(engine.awayPlayerStats(), exported.away);
    const std::string homeBatters = battersJson(engine.homePlayerStats(), exported.home);
    out << awayBatters.substr(1, awayBatters.size() - 2);
    if (awayBatters.size() > 2 && homeBatters.size() > 2) out << ",";
    out << homeBatters.substr(1, homeBatters.size() - 2);
    out << "],\n";
    out << "    \"pitchers\":";
    out << "[";
    const std::string awayPitchers = pitchersJson(engine.awayPitcherStats(), exported.away);
    const std::string homePitchers = pitchersJson(engine.homePitcherStats(), exported.home);
    out << awayPitchers.substr(1, awayPitchers.size() - 2);
    if (awayPitchers.size() > 2 && homePitchers.size() > 2) out << ",";
    out << homePitchers.substr(1, homePitchers.size() - 2);
    out << "]\n";
    out << "  },\n";
    out << "  \"events\":[\n";
    for (std::size_t i = 0; i < events.size(); ++i) {
        out << "    " << events[i];
        if (i + 1 < events.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    exported.json = out.str();
    return exported;
}

std::vector<GameExport> generateSeason(const std::vector<joji::Team>& teams,
                                       int season,
                                       int limit) {
    std::vector<GameExport> games;
    std::vector<int> teamGames(teams.size(), 0);
    int gameNumber = 0;

    for (int i = 0; i < static_cast<int>(teams.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(teams.size()); ++j) {
            const int seriesLen = (teams[i].league() == teams[j].league()) ? kIntraSeries : kInterSeries;
            for (int g = 0; g < seriesLen; ++g) {
                if (limit > 0 && static_cast<int>(games.size()) >= limit) return games;
                const bool flipHome = (g % 2) == 1;
                const int awayIdx = flipHome ? j : i;
                const int homeIdx = flipHome ? i : j;
                const int awaySlot = teamGames[static_cast<std::size_t>(awayIdx)] % 5;
                const int homeSlot = teamGames[static_cast<std::size_t>(homeIdx)] % 5;
                teamGames[static_cast<std::size_t>(awayIdx)]++;
                teamGames[static_cast<std::size_t>(homeIdx)]++;

                ++gameNumber;
                const std::string gameId = std::to_string(season) + "-" +
                    (gameNumber < 1000 ? std::string(3 - std::to_string(gameNumber).size(), '0') : "") +
                    std::to_string(gameNumber);
                const uint32_t seed = static_cast<uint32_t>(season * 100000 + gameNumber);
                games.push_back(simulateGame(teams, awayIdx, homeIdx, season, seed,
                                             awaySlot, homeSlot, dateForGame(season, gameNumber - 1), gameId));
            }
        }
    }

    return games;
}

std::string gamesIndexJson(const std::vector<GameExport>& games) {
    std::ostringstream out;
    out << "[\n";
    for (std::size_t i = 0; i < games.size(); ++i) {
        const auto& g = games[i];
        out << "  " << joji::jsonString(g.gameId + ".json");
        if (i + 1 < games.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

std::string seasonJson(const std::vector<joji::Team>& teams,
                       const std::vector<GameExport>& games,
                       int season) {
    std::vector<TeamStanding> standings(teams.size());
    for (const auto& g : games) {
        TeamStanding& away = standings[static_cast<std::size_t>(g.awayIdx)];
        TeamStanding& home = standings[static_cast<std::size_t>(g.homeIdx)];
        away.rs += g.awayScore;
        away.ra += g.homeScore;
        home.rs += g.homeScore;
        home.ra += g.awayScore;
        if (g.awayScore > g.homeScore) {
            away.wins++;
            home.losses++;
        } else if (g.homeScore > g.awayScore) {
            home.wins++;
            away.losses++;
        }
    }

    std::ostringstream out;
    out << "{\n";
    out << "  \"schemaVersion\":1,\n";
    out << "  \"season\":" << season << ",\n";
    out << "  \"league\":\"JBL\",\n";
    out << "  \"generatedAt\":" << joji::jsonString(todayDate()) << ",\n";
    out << "  \"teams\":[\n";
    for (std::size_t i = 0; i < teams.size(); ++i) {
        const auto& t = teams[i];
        const auto& s = standings[i];
        const int gamesPlayed = s.wins + s.losses;
        const double pct = gamesPlayed > 0 ? static_cast<double>(s.wins) / gamesPlayed : 0.0;
        out << std::fixed << std::setprecision(3)
            << "    {\"name\":" << joji::jsonString(t.name())
            << ",\"division\":" << joji::jsonString(t.league() == joji::League::A ? "League A" : "League B")
            << ",\"slug\":" << joji::jsonString(teamSlug(t.name()))
            << ",\"wins\":" << s.wins
            << ",\"losses\":" << s.losses
            << ",\"pct\":" << pct
            << std::setprecision(2)
            << ",\"rs\":" << s.rs
            << ",\"ra\":" << s.ra
            << ",\"rsPerGame\":" << (gamesPlayed > 0 ? static_cast<double>(s.rs) / gamesPlayed : 0.0)
            << ",\"raPerGame\":" << (gamesPlayed > 0 ? static_cast<double>(s.ra) / gamesPlayed : 0.0)
            << "}";
        if (i + 1 < teams.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"players\":[\n";
    bool firstPlayer = true;
    for (const auto& team : teams) {
        for (const auto& player : team.lineup()) {
            if (!firstPlayer) out << ",\n";
            firstPlayer = false;
            out << "    {\"name\":" << joji::jsonString(player.name)
                << ",\"team\":" << joji::jsonString(team.name())
                << ",\"age\":" << player.age
                << ",\"position\":" << joji::jsonString(positionLabel(player.position))
                << "}";
        }
    }
    out << "\n  ],\n";
    out << "  \"battingLeaders\":[],\n";
    out << "  \"pitchingLeaders\":[],\n";
    out << "  \"schedule\":[\n";
    for (std::size_t i = 0; i < games.size(); ++i) {
        const auto& g = games[i];
        out << "    {\"gameId\":" << joji::jsonString(g.gameId)
            << ",\"date\":" << joji::jsonString(g.date)
            << ",\"away\":" << joji::jsonString(g.away)
            << ",\"home\":" << joji::jsonString(g.home)
            << ",\"venue\":" << joji::jsonString(g.venue)
            << ",\"status\":\"final\""
            << ",\"finalScore\":{\"away\":" << g.awayScore << ",\"home\":" << g.homeScore << "}"
            << ",\"gameFile\":" << joji::jsonString(g.gameFile)
            << "}";
        if (i + 1 < games.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

CliOptions parseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--season" && i + 1 < argc) {
            opts.seasonMode = true;
            opts.season = std::stoi(argv[++i]);
        } else if (arg == "--all-games") {
            opts.allGames = true;
        } else if (arg == "--out-dir" && i + 1 < argc) {
            opts.outDir = argv[++i];
        } else if (arg == "--limit" && i + 1 < argc) {
            opts.limit = std::stoi(argv[++i]);
        } else if (!arg.empty() && arg[0] != '-') {
            opts.seed = static_cast<uint32_t>(std::stoul(arg));
        }
    }
    return opts;
}

void writeTextFile(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Unable to write " + path.string());
    }
    out << contents;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const CliOptions opts = parseArgs(argc, argv);
        const auto teams = joji::allTeams();

        if (opts.seasonMode || opts.allGames) {
            if (!opts.allGames || opts.outDir.empty()) {
                std::cerr << "Usage: ./JojiGameExport --season YEAR --all-games --out-dir DIR [--limit N]\n";
                return 2;
            }

            const std::vector<GameExport> games = generateSeason(teams, opts.season, opts.limit);
            const std::filesystem::path root(opts.outDir);
            const std::filesystem::path gamesDir = root / "games";
            std::filesystem::create_directories(gamesDir);

            for (const auto& game : games) {
                writeTextFile(root / game.gameFile, game.json);
            }
            writeTextFile(gamesDir / "index.json", gamesIndexJson(games));
            writeTextFile(root / "season.json", seasonJson(teams, games, opts.season));

            std::cerr << "Wrote " << games.size() << " game JSON files to "
                      << root.string() << "\n";
            return 0;
        }

        uint32_t seed = opts.seed.value_or(0);
        if (seed == 0) {
            const std::time_t t = std::time(nullptr);
            std::tm* tm = std::localtime(&t);
            seed = static_cast<uint32_t>(
                (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday);
        }

        const int n = static_cast<int>(teams.size());
        const int awayIdx = static_cast<int>((seed / 13) % n);
        int homeIdx = (awayIdx + static_cast<int>(seed % 5) + 1) % n;
        if (homeIdx == awayIdx) homeIdx = (awayIdx + 1) % n;

        const GameExport game = simulateGame(teams, awayIdx, homeIdx, 0, seed,
                                             static_cast<int>(seed % 5),
                                             static_cast<int>((seed + 2) % 5),
                                             todayDate(), "game-" + std::to_string(seed));
        std::cout << game.json;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "JojiGameExport error: " << ex.what() << "\n";
        return 1;
    }
}
