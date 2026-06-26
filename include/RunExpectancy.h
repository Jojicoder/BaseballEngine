#pragma once

#include "GameState.h"

namespace joji {

// 24-state run expectancy table. Values are runs expected from the current
// base/out state to the end of the inning, calibrated to a modern scoring band.
double runExpectancy(int outs, bool onFirst, bool onSecond, bool onThird);
double runExpectancy(const GameState& state);

struct RunExpectancyDelta {
    double current = 0.0;
    double success = 0.0;
    double failure = 0.0;
    double breakEvenSuccessRate = 1.0;
};

RunExpectancyDelta stealRunExpectancyDelta(const GameState& state,
                                           int fromBase,
                                           int toBase);

} // namespace joji
