#pragma once

#include "AtBatTypes.h"

namespace joji {

struct UmpireProfile {
    double horizBias = 0.0; // + = wider zone horizontally
    double vertBias  = 0.0; // + = higher zone top
};

class ZoneJudge {
public:
    ZoneJudge() = default;
    explicit ZoneJudge(UmpireProfile ump) : ump_(ump) {}

    ZoneResult judge(const Pitch& pitch) const;

private:
    UmpireProfile ump_;
};

} // namespace joji
