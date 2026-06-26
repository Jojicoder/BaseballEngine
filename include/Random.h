#pragma once

#include <cstdint>
#include <optional>
#include <random>

namespace joji {

bool chanceFromRoll(double probability, double roll);

class Random {
public:
    explicit Random(std::optional<std::uint32_t> seed = std::nullopt);

    double real(double minInclusive, double maxExclusive);
    int integer(int minInclusive, int maxInclusive);
    bool chance(double probability);

private:
    std::mt19937 engine_;
};

} // namespace joji
