#pragma once

#include "AtBatTypes.h"
#include "Player.h"
#include "Random.h"

namespace joji {

class ContactEngine {
public:
    ContactResult resolve(const Player& batter,
                          const Player& pitcher,
                          const Pitch& pitch,
                          const Swing& swing,
                          const Count& count,
                          Random& random) const;
};

} // namespace joji
