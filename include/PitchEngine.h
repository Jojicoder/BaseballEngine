#pragma once

#include "AtBatTypes.h"
#include "Player.h"
#include "Random.h"

#include <optional>

namespace joji {

class PitchEngine {
public:
    Pitch generate(const Player& pitcher, const Player& batter, const Count& count,
                   const std::optional<Pitch>& lastPitch,
                   const std::optional<BatterHistory>& history,
                   Random& random) const;
};

} // namespace joji
