#pragma once

namespace joji {

struct GameRules {
    int maxInnings = 18;
    bool useExtraInningTiebreaker = true;
    int tiebreakerStartInning = 10;
};

} // namespace joji
