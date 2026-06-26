#include "RunExpectancy.h"

#include <algorithm>
#include <array>

namespace joji {
namespace {

int baseMask(bool onFirst, bool onSecond, bool onThird) {
    return (onFirst ? 1 : 0) | (onSecond ? 2 : 0) | (onThird ? 4 : 0);
}

double tableValue(int outs, int mask) {
    static constexpr std::array<std::array<double, 8>, 3> Table{{
        // mask: 0=---, 1=1--, 2=-2-, 3=12-, 4=--3, 5=1-3, 6=-23, 7=123
        {{0.48,   0.86,   1.10,   1.44,   1.35,  1.78,  1.96,  2.29}},
        {{0.25,   0.51,   0.66,   0.88,   0.95,  1.13,  1.38,  1.54}},
        {{0.10,   0.22,   0.32,   0.43,   0.35,  0.48,  0.58,  0.75}},
    }};
    if (outs < 0 || outs >= 3) {
        return 0.0;
    }
    return Table[static_cast<std::size_t>(outs)][static_cast<std::size_t>(mask & 7)];
}

bool occupied(const GameState& state, int base) {
    return base >= 1 && base <= 3
        && state.bases[static_cast<std::size_t>(base - 1)].has_value();
}

int maskForState(const GameState& state) {
    return baseMask(occupied(state, 1), occupied(state, 2), occupied(state, 3));
}

} // namespace

double runExpectancy(int outs, bool onFirst, bool onSecond, bool onThird) {
    return tableValue(outs, baseMask(onFirst, onSecond, onThird));
}

double runExpectancy(const GameState& state) {
    return tableValue(state.outs, maskForState(state));
}

RunExpectancyDelta stealRunExpectancyDelta(const GameState& state,
                                           int fromBase,
                                           int toBase) {
    RunExpectancyDelta delta;
    delta.current = runExpectancy(state);

    if (state.outs >= 3 || fromBase < 1 || fromBase > 3 || toBase < 2 || toBase > 3
        || toBase <= fromBase || !occupied(state, fromBase) || occupied(state, toBase)) {
        delta.success = delta.current;
        delta.failure = 0.0;
        delta.breakEvenSuccessRate = 1.0;
        return delta;
    }

    int successMask = maskForState(state);
    successMask &= ~(1 << (fromBase - 1));
    successMask |= (1 << (toBase - 1));
    delta.success = tableValue(state.outs, successMask);

    int failureMask = maskForState(state);
    failureMask &= ~(1 << (fromBase - 1));
    delta.failure = tableValue(std::min(state.outs + 1, 3), failureMask);

    const double gain = delta.success - delta.failure;
    delta.breakEvenSuccessRate = gain > 0.001
        ? std::clamp((delta.current - delta.failure) / gain, 0.0, 1.0)
        : 1.0;
    return delta;
}

} // namespace joji
