#include "GameEngine.h"
#include "Teams.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

std::optional<std::uint32_t> parseSeed(int argc, char* argv[]) {
    if (argc < 2) {
        return std::nullopt;
    }

    try {
        return static_cast<std::uint32_t>(std::stoul(argv[1]));
    } catch (const std::exception&) {
        std::cerr << "Invalid seed '" << argv[1] << "'. Running with non-deterministic seed.\n";
        return std::nullopt;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    auto seed = parseSeed(argc, argv);
    const auto teams = joji::allTeams();
    joji::Random random{seed};
    joji::GameEngine engine{teams.at(0), teams.at(1), std::move(random)};
    engine.simulate(std::cout);
    return 0;
}
