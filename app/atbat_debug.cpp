#include "AtBatEngine.h"
#include "BallPhysicsEngine.h"
#include "GameState.h"
#include "PlayResolutionEngine.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace {

// ── プレイヤー定義 ───────────────────────────────────────────
joji::Player makeBatter(const std::string& name, joji::BattingSide side) {
    joji::Player p;
    p.name = name;
    p.contact = 72; p.power = 64; p.eye = 68; p.speed = 74;
    p.fielding = 61; p.arm = 55;
    p.battingSide = side;
    return p;
}

joji::Player makePitcher(const std::string& name, joji::ThrowingHand hand) {
    joji::Player p;
    p.name = name;
    p.pitchingVelocity = 82; p.pitchingControl = 61; p.pitchingStuff = 77;
    p.throwingHand = hand;
    return p;
}

// ── ヘルパー ─────────────────────────────────────────────────
std::string handStr(joji::ThrowingHand h) {
    return h == joji::ThrowingHand::Right ? "RHP" : "LHP";
}
std::string sideStr(joji::BattingSide s) {
    return s == joji::BattingSide::Right ? "RHB" : "LHB";
}
std::string pitchLabel(joji::PitchType t) {
    return joji::toString(t).substr(0, 3);
}

bool inStrikeZone(const joji::Pitch& p) {
    return std::abs(p.locationX) <= 0.83 && p.locationZ >= 1.55 && p.locationZ <= 3.50;
}
bool nearZone(const joji::Pitch& p) {
    return std::abs(p.locationX) <= 1.05 && p.locationZ >= 1.30 && p.locationZ <= 3.75;
}

// ── 集計 ─────────────────────────────────────────────────────
struct Stats {
    std::map<joji::PitchType, int> pitchTypeCounts;
    int totalPitches = 0;
    int chaseOpportunities = 0;  // ball pitches near zone
    int chases = 0;
    int swingInZone = 0;
    int takeInZone = 0;           // called strike / take
    double swingProbSum = 0.0;
    int swings = 0;
    int misses = 0;
    int fouls = 0;
    int inPlay = 0;
    int barrelCount = 0;          // barrelAccuracy > 0.72 and InPlay
    int jammedCount = 0;          // contactResult.isJammed and InPlay
    std::vector<double> sprayAngles;
};

void runMatchup(const joji::Player& batter,
                const joji::Player& pitcher,
                int paCount,
                joji::Random& random,
                Stats& stats) {
    joji::AtBatEngine engine;
    for (int i = 0; i < paCount; ++i) {
        const joji::AtBatResult result = engine.simulatePlateAppearance(batter, pitcher, random);
        for (const auto& log : result.pitchLogs) {
            ++stats.totalPitches;
            ++stats.pitchTypeCounts[log.pitch.pitchType];
            stats.swingProbSum += log.swingDecision.swingProbability;

            const bool zone = inStrikeZone(log.pitch);
            const bool near = !zone && nearZone(log.pitch);
            const bool swung = log.swingDecision.decision == joji::SwingDecisionType::Swing;

            if (near) {
                ++stats.chaseOpportunities;
                if (swung) ++stats.chases;
            }
            if (zone) {
                if (swung) ++stats.swingInZone;
                else       ++stats.takeInZone;
            }
            if (swung) ++stats.swings;

            if (log.contactResult.has_value()) {
                const auto& cr = *log.contactResult;
                if (cr.resultType == joji::ContactResultType::SwingMiss) ++stats.misses;
                if (cr.resultType == joji::ContactResultType::Foul)      ++stats.fouls;
                if (cr.resultType == joji::ContactResultType::InPlay) {
                    ++stats.inPlay;
                    if (cr.isJammed) ++stats.jammedCount;
                    if (cr.barrelAccuracy > 0.70) ++stats.barrelCount;
                    if (cr.battedBallInput.has_value()) {
                        stats.sprayAngles.push_back(cr.battedBallInput->sprayAngle);
                    }
                }
            }
        }
    }
}

void printStats(const joji::Player& batter,
                const joji::Player& pitcher,
                int paCount,
                const Stats& s) {
    const bool same = (pitcher.throwingHand == joji::ThrowingHand::Right && batter.battingSide == joji::BattingSide::Right)
                   || (pitcher.throwingHand == joji::ThrowingHand::Left  && batter.battingSide == joji::BattingSide::Left);
    const std::string matchLabel = handStr(pitcher.throwingHand) + " vs " + sideStr(batter.battingSide)
                                   + (same ? "  [same-handed]" : "  [opposite]");

    std::cout << "\n══════════════════════════════════════════════════\n"
              << matchLabel << "  |  " << paCount << " PA\n"
              << "══════════════════════════════════════════════════\n";

    // Pitch type distribution
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "PitchType  : ";
    for (auto type : {joji::PitchType::Fastball, joji::PitchType::Slider, joji::PitchType::Curveball,
                      joji::PitchType::Changeup, joji::PitchType::Cutter, joji::PitchType::Splitter}) {
        const int n = s.pitchTypeCounts.count(type) ? s.pitchTypeCounts.at(type) : 0;
        const double pct = s.totalPitches > 0 ? 100.0 * n / s.totalPitches : 0.0;
        std::cout << pitchLabel(type) << " " << std::setw(4) << pct << "%  ";
    }
    std::cout << "\n";

    // Swing decision
    const double chaseRate = s.chaseOpportunities > 0 ? 100.0 * s.chases / s.chaseOpportunities : 0.0;
    const double zoneSwing = (s.swingInZone + s.takeInZone) > 0
                           ? 100.0 * s.swingInZone / (s.swingInZone + s.takeInZone) : 0.0;
    const double avgSwingP = s.totalPitches > 0 ? s.swingProbSum / s.totalPitches : 0.0;
    std::cout << "SwingDecision: "
              << "chase " << std::setw(4) << chaseRate << "%  "
              << "zone-swing " << std::setw(4) << zoneSwing << "%  "
              << "avg-p " << std::setprecision(3) << avgSwingP << "\n";

    // Contact results
    if (s.swings > 0) {
        const double missRate   = 100.0 * s.misses  / s.swings;
        const double foulRate   = 100.0 * s.fouls   / s.swings;
        const double inPlayRate = 100.0 * s.inPlay  / s.swings;
        std::cout << std::setprecision(1);
        std::cout << "Contact    : "
                  << "miss " << std::setw(4) << missRate << "%  "
                  << "foul " << std::setw(4) << foulRate << "%  "
                  << "inPlay " << std::setw(4) << inPlayRate << "%\n";
    }
    if (s.inPlay > 0) {
        const double barrelRate = 100.0 * s.barrelCount / s.inPlay;
        const double jammedRate = 100.0 * s.jammedCount / s.inPlay;
        std::cout << "InPlay     : "
                  << "barrel " << std::setw(4) << barrelRate << "%  "
                  << "jammed " << std::setw(4) << jammedRate << "%\n";
    }

    // Spray angle
    if (!s.sprayAngles.empty()) {
        const double mean = std::accumulate(s.sprayAngles.begin(), s.sprayAngles.end(), 0.0) / s.sprayAngles.size();
        int pull = 0;
        for (double a : s.sprayAngles) {
            // RHB pull = negative, LHB pull = positive
            if (batter.battingSide == joji::BattingSide::Right && a < -5.0) ++pull;
            if (batter.battingSide == joji::BattingSide::Left  && a >  5.0) ++pull;
        }
        const double pullPct = 100.0 * pull / s.sprayAngles.size();
        std::cout << "SprayAngle : "
                  << "mean " << std::setw(5) << std::setprecision(1) << mean << "°  "
                  << "pull " << std::setw(4) << pullPct << "%"
                  << "  (RHB pull=left<0, LHB pull=right>0)\n";
    }
}

std::optional<std::uint32_t> parseSeed(int argc, char* argv[]) {
    if (argc >= 2) {
        try { return static_cast<std::uint32_t>(std::stoul(argv[1])); }
        catch (...) {}
    }
    return 42u;
}

} // namespace

int main(int argc, char* argv[]) {
    constexpr int PA = 400;
    joji::Random random{parseSeed(argc, argv)};

    const std::pair<joji::ThrowingHand, joji::BattingSide> combos[] = {
        { joji::ThrowingHand::Right, joji::BattingSide::Right },  // RHP vs RHB  same
        { joji::ThrowingHand::Right, joji::BattingSide::Left  },  // RHP vs LHB  opposite
        { joji::ThrowingHand::Left,  joji::BattingSide::Right },  // LHP vs RHB  opposite
        { joji::ThrowingHand::Left,  joji::BattingSide::Left  },  // LHP vs LHB  same
    };

    std::cout << "atbat_debug  |  " << PA << " PA per matchup\n";

    for (const auto& [hand, side] : combos) {
        const joji::Player batter  = makeBatter("Joji Rivera", side);
        const joji::Player pitcher = makePitcher("Victor Hale", hand);
        Stats stats;
        runMatchup(batter, pitcher, PA, random, stats);
        printStats(batter, pitcher, PA, stats);
    }

    std::cout << "\n";
    return 0;
}
