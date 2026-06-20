#pragma once

#include "AtBatTypes.h"

namespace joji {

class ZoneJudge {
public:
    ZoneResult judge(const Pitch& pitch) const;
};

} // namespace joji
