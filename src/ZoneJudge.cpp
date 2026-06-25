#include "ZoneJudge.h"

#include <cmath>

namespace joji {

ZoneResult ZoneJudge::judge(const Pitch& pitch) const {
    ZoneResult result;
    result.locationX = pitch.locationX;
    result.locationZ = pitch.locationZ;

    const double xLimit  = 0.73 + ump_.horizBias + ump_.framingBias;
    const double zHigh   = 3.50 + ump_.vertBias  + ump_.framingBias * 0.5;
    const bool insideWidth  = std::abs(pitch.locationX) <= xLimit;
    const bool insideHeight = pitch.locationZ >= 1.55 && pitch.locationZ <= zHigh;
    result.result = insideWidth && insideHeight ? ZoneResultType::CalledStrike : ZoneResultType::Ball;
    return result;
}

} // namespace joji
