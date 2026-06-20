#pragma once

#include "AtBatTypes.h"
#include "Player.h"
#include "Random.h"

namespace joji {

class SwingDecisionEngine {
public:
    SwingDecision decide(const Player& batter, const Pitch& pitch, const Count& count, ThrowingHand pitcherHand, Random& random) const;
};

} // namespace joji
