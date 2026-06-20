#pragma once

#include "AtBatTypes.h"
#include "Player.h"
#include "Random.h"

namespace joji {

class SwingEngine {
public:
    Swing generate(const Player& batter,
                   const Pitch& pitch,
                   const Count& count,
                   const SwingDecision& decision,
                   Random& random) const;
};

} // namespace joji
