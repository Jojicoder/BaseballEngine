#pragma once

#include "PlayResult.h"
#include "PlayResolutionEngine.h"
#include "Random.h"

namespace joji {

struct BattedBallInput {
    double exitVelocity = 90.0;
    double launchAngle = 15.0;
    double sprayAngle = 0.0;
    double sideSpin = 0.0;
    double backSpin = 1800.0;
    bool isBunt = false;
};

class BallPhysicsEngine {
public:
    BattedBall simulate(const BattedBallInput& input,
                        const BallparkConfig& ballpark = BallparkConfig{},
                        bool fastMode = false) const;
    BattedBall generateFromContact(int batterPower, int pitcherStuff,
                                   AtBatResultType result, Random& random) const;

private:
    static std::string classify(const BattedBall& ball, bool foul,
                                const BallparkConfig& ballpark);
};

} // namespace joji
