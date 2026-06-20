#include "Random.h"

#include <chrono>

namespace joji {

Random::Random(std::optional<std::uint32_t> seed) {
    if (seed.has_value()) {
        engine_.seed(*seed);
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::random_device device;
    std::seed_seq seedSequence{
        static_cast<std::uint32_t>(device()),
        static_cast<std::uint32_t>(now),
        static_cast<std::uint32_t>(now >> 32)
    };
    engine_.seed(seedSequence);
}

double Random::real(double minInclusive, double maxExclusive) {
    std::uniform_real_distribution<double> distribution(minInclusive, maxExclusive);
    return distribution(engine_);
}

int Random::integer(int minInclusive, int maxInclusive) {
    std::uniform_int_distribution<int> distribution(minInclusive, maxInclusive);
    return distribution(engine_);
}

bool Random::chance(double probability) {
    if (probability <= 0.0) {
        return false;
    }
    if (probability >= 1.0) {
        return true;
    }
    return real(0.0, 1.0) < probability;
}

} // namespace joji
