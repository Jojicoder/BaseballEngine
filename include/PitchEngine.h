#pragma once

#include "AtBatTypes.h"
#include "Player.h"
#include "Random.h"

namespace joji {

class PitchEngine {
public:
    Pitch generate(const Player& pitcher, const Count& count, BattingSide batterSide, Random& random) const;
};

} // namespace joji
