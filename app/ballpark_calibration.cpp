#include "BallPhysicsEngine.h"
#include "PlayResolutionEngine.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

// 標準バックスピン: LA が高いほど多い (実測近似)
double standardBackSpin(double launchAngle) {
    return 1500.0 + launchAngle * 35.0;
}

struct Shot {
    double ev;
    double la;
    double dist;
    double maxH;
    double hangTime;
    bool crossesFence;
    double fenceCrossHeight;
};

Shot shoot(joji::BallPhysicsEngine& engine,
           double ev, double la, double spray,
           const joji::BallparkConfig& park) {
    joji::BattedBallInput input;
    input.exitVelocity = ev;
    input.launchAngle  = la;
    input.sprayAngle   = spray;
    input.backSpin     = standardBackSpin(la);
    input.sideSpin     = 0.0;

    const joji::BattedBall ball = engine.simulate(input, park);
    return {ev, la, ball.estimatedDistance, ball.maxHeight,
            ball.hangTime, ball.crossesFence, ball.fenceCrossHeight};
}

void header(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

} // namespace

int main() {
    joji::BallPhysicsEngine engine;
    const joji::BallparkConfig park = joji::BallparkConfig::generic();

    std::cout << "BallPhysics Calibration  |  Generic Park\n";
    std::cout << "CF " << park.centerFieldFenceFeet << "ft  "
              << "Gap " << park.gapFenceFeet << "ft  "
              << "Corner " << park.cornerFenceFeet << "ft  "
              << "Fence " << park.fenceHeightFeet << "ft\n";

    // ── 1. EV × LA グリッド (sprayAngle=0) ──────────────────────
    header("Distance Table  EV(mph) × LA(deg)  —  sprayAngle=0");

    const std::vector<double> evList  = {80, 85, 90, 95, 98, 100, 103, 105, 108, 110};
    const std::vector<double> laList  = {10, 15, 20, 25, 28, 30,  35,  40};

    // ヘッダ行
    std::cout << std::right << std::setw(6) << "EV\\LA";
    for (double la : laList)
        std::cout << std::setw(6) << static_cast<int>(la) << "°";
    std::cout << "\n" << std::string(6 + laList.size()*6, '-') << "\n";

    for (double ev : evList) {
        std::cout << std::setw(5) << static_cast<int>(ev) << " ";
        for (double la : laList) {
            Shot s = shoot(engine, ev, la, 0.0, park);
            std::string dist = std::to_string(static_cast<int>(std::round(s.dist)));
            if (s.crossesFence) dist += "*";
            std::cout << std::setw(6) << dist;
        }
        std::cout << "\n";
    }
    std::cout << "  (* = HR)\n";

    // ── 2. Statcast 基準との比較 ──────────────────────────────────
    header("Key Shots vs. Statcast Reference  (LA=28°, spray=0)");
    std::cout << std::left  << std::setw(8)  << "EV"
              << std::right << std::setw(8)  << "Dist(ft)"
              << std::setw(8)  << "MaxH(ft)"
              << std::setw(8)  << "Hang(s)"
              << std::setw(6)  << "HR?"
              << "  Statcast ref\n";
    std::cout << std::string(54, '-') << "\n";

    struct Ref { double ev; const char* expected; };
    const std::vector<Ref> refs = {
        {90,  "~290-310 ft  (single/flyout)"},
        {95,  "~330-360 ft  (warning track / HR edge)"},
        {98,  "~360-390 ft  (HR center)"},
        {100, "~370-400 ft  (HR)"},
        {103, "~390-420 ft  (solid HR)"},
        {105, "~400-430 ft  (deep HR)"},
        {110, "~420-460 ft  (monster)"},
    };
    for (const auto& r : refs) {
        Shot s = shoot(engine, r.ev, 28.0, 0.0, park);
        std::cout << std::left  << std::setw(8)  << (std::to_string(static_cast<int>(r.ev)) + " mph")
                  << std::right << std::setw(8)  << static_cast<int>(std::round(s.dist))
                  << std::setw(8)  << static_cast<int>(std::round(s.maxH))
                  << std::fixed << std::setprecision(2)
                  << std::setw(8)  << s.hangTime
                  << std::setw(6)  << (s.crossesFence ? "HR" : "---")
                  << "  " << r.expected << "\n";
    }

    // ── 3. HR 境界: EV別の最小 LA ────────────────────────────────
    header("HR Boundary  —  minimum LA to clear CF fence");
    std::cout << std::left  << std::setw(8)  << "EV"
              << std::right << std::setw(10) << "Min LA(°)"
              << std::setw(10) << "Dist@HR"
              << "\n";
    std::cout << std::string(28, '-') << "\n";

    for (double ev : {85.0, 90.0, 95.0, 98.0, 100.0, 103.0, 105.0, 110.0}) {
        int minLA = -1;
        double distAtHR = 0.0;
        for (int la = 8; la <= 50; ++la) {
            Shot s = shoot(engine, ev, static_cast<double>(la), 0.0, park);
            if (s.crossesFence) {
                minLA = la;
                distAtHR = s.dist;
                break;
            }
        }
        std::cout << std::left  << std::setw(8) << (std::to_string(static_cast<int>(ev)) + " mph")
                  << std::right << std::setw(10) << (minLA >= 0 ? std::to_string(minLA)+"°" : "none")
                  << std::setw(10) << (minLA >= 0 ? std::to_string(static_cast<int>(distAtHR))+"ft" : "---")
                  << "\n";
    }

    // ── 4. LA スイープ (EV=100, spray=0) ─────────────────────────
    header("LA Sweep  EV=100mph  spray=0°");
    std::cout << std::left  << std::setw(8)  << "LA"
              << std::right << std::setw(8)  << "Dist"
              << std::setw(8)  << "MaxH"
              << std::setw(8)  << "Hang"
              << std::setw(6)  << "HR?"
              << "\n";
    std::cout << std::string(40, '-') << "\n";
    for (int la = 5; la <= 45; la += 5) {
        Shot s = shoot(engine, 100.0, static_cast<double>(la), 0.0, park);
        std::cout << std::left  << std::setw(8)  << (std::to_string(la) + "°")
                  << std::right << std::setw(8)  << static_cast<int>(std::round(s.dist))
                  << std::setw(8)  << static_cast<int>(std::round(s.maxH))
                  << std::fixed << std::setprecision(2)
                  << std::setw(8)  << s.hangTime
                  << std::setw(6)  << (s.crossesFence ? "HR" : "---")
                  << "\n";
    }

    // ── 5. Spray 影響 (EV=100, LA=28) ────────────────────────────
    header("Spray  EV=100mph  LA=28°");
    std::cout << std::left  << std::setw(10) << "Spray"
              << std::right << std::setw(8)  << "Dist"
              << std::setw(8)  << "HR?"
              << "  Direction\n";
    std::cout << std::string(38, '-') << "\n";
    for (int spray : {-40, -30, -20, -10, 0, 10, 20, 30, 40}) {
        Shot s = shoot(engine, 100.0, 28.0, static_cast<double>(spray), park);
        const char* dir = spray < -20 ? "oppo corner"
                        : spray < 0   ? "oppo gap"
                        : spray == 0  ? "dead center"
                        : spray < 20  ? "pull gap"
                                      : "pull corner";
        std::cout << std::left  << std::setw(10) << (std::to_string(spray) + "°")
                  << std::right << std::setw(8)  << static_cast<int>(std::round(s.dist))
                  << std::fixed << std::setprecision(1)
                  << std::setw(8)  << s.fenceCrossHeight
                  << std::setw(8)  << (s.crossesFence ? "HR" : "---")
                  << "  " << dir << "\n";
    }

    return 0;
}
