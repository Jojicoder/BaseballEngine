#include "GameEngine.h"
#include "JsonExporter.h"
#include <iostream>
#include <sstream>
#include <cstdint>

namespace {

joji::Team createNewarkKnights() {
    return joji::Team{
        "Newark Knights",
        {
            {"Joji Rivera",   72, 64, 68, 74, 61, 55, 35, 34, 32},
            {"Marcus Bell",   66, 71, 58, 62, 70, 67, 30, 30, 30},
            {"Andre Vale",    61, 82, 54, 48, 58, 60, 36, 35, 34},
            {"Nico Sterling", 74, 57, 70, 69, 64, 58, 28, 32, 29},
            {"Dante Cruz",    58, 76, 49, 55, 66, 72, 34, 31, 30},
            {"Eli Brooks",    63, 61, 62, 71, 73, 64, 30, 31, 29},
            {"Tariq Mason",   55, 68, 52, 59, 62, 76, 32, 30, 31},
            {"Cal Weston",    60, 52, 65, 63, 69, 57, 35, 38, 36},
            {"Rafael Stone",  43, 45, 42, 39, 52, 61, 78, 66, 73}
        }
    };
}

joji::Team createQueensTitans() {
    return joji::Team{
        "Queens Titans",
        {
            {"Malik Chen",    69, 58, 71, 77, 68, 59, 32, 31, 30},
            {"Oscar Vega",    64, 73, 57, 61, 63, 66, 34, 34, 31},
            {"Theo Grant",    73, 79, 63, 52, 57, 62, 35, 32, 33},
            {"Julian Frost",  62, 84, 51, 46, 54, 60, 36, 34, 35},
            {"Samir Holt",    70, 60, 67, 68, 71, 63, 29, 31, 30},
            {"Isaac Monroe",  59, 69, 55, 57, 65, 74, 31, 32, 32},
            {"Leo Navarro",   57, 63, 60, 72, 76, 69, 33, 34, 30},
            {"Miles Archer",  65, 54, 64, 66, 67, 58, 32, 35, 34},
            {"Victor Hale",   41, 48, 45, 38, 50, 64, 82, 61, 77}
        }
    };
}

} // namespace

int main() {
    joji::GameEngine engine{
        createNewarkKnights(),
        createQueensTitans(),
        joji::Random{42}
    };

    std::ostringstream sink;
    engine.simulate(sink);

    std::cout << joji::exportGameToJson(engine);
    return 0;
}
