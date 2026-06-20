#include "ZoneJudge.h"

#include <cmath>

namespace joji {

ZoneResult ZoneJudge::judge(const Pitch& pitch) const {
    ZoneResult result;
    result.locationX = pitch.locationX;
    result.locationZ = pitch.locationZ;

    const bool insideWidth = std::abs(pitch.locationX) <= 0.83;
    const bool insideHeight = pitch.locationZ >= 1.55 && pitch.locationZ <= 3.50;
    result.result = insideWidth && insideHeight ? ZoneResultType::CalledStrike : ZoneResultType::Ball;
    return result;
}

} // namespace joji
