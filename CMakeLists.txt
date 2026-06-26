cmake_minimum_required(VERSION 3.16)

project(JojiBaseballEngine
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JOJI_ENGINE_SOURCES
    src/AtBatTypes.cpp
    src/AnimationPlanBuilder.cpp
    src/AtBatEngine.cpp
    src/BallPhysicsEngine.cpp
    src/ContactEngine.cpp
    src/GameEngine.cpp
    src/GameEngineBaseRunning.cpp
    src/JsonExporter.cpp
    src/PitchEngine.cpp
    src/PlayResolutionEngine.cpp
    src/Random.cpp
    src/RunExpectancy.cpp
    src/SwingDecisionEngine.cpp
    src/SwingEngine.cpp
    src/Teams.cpp
    src/ZoneJudge.cpp
)

add_library(JojiEngineCore STATIC ${JOJI_ENGINE_SOURCES})
target_include_directories(JojiEngineCore PUBLIC include)

if (MSVC)
    target_compile_options(JojiEngineCore PRIVATE /W4)
else()
    target_compile_options(JojiEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(joji_add_app target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE JojiEngineCore)
endfunction()

joji_add_app(JojiBaseballEngine app/main.cpp)
joji_add_app(JojiEngineTests app/engine_tests.cpp app/json_tests.cpp)
joji_add_app(JojiAtBatDebug app/atbat_debug.cpp)
joji_add_app(JojiStatsRunner app/stats_runner.cpp)
joji_add_app(JojiAnalysisRunner app/analysis_runner.cpp)
joji_add_app(JojiSeasonRunner app/season_runner.cpp)
joji_add_app(JojiLeagueRunner app/league_runner.cpp)
joji_add_app(JojiPitchCheck app/pitch_scoring_check.cpp)

find_package(Threads REQUIRED)
target_link_libraries(JojiSeasonRunner PRIVATE Threads::Threads)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/game_export.cpp")
    joji_add_app(JojiGameExport app/game_export.cpp)
endif()

find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
if (SFML_FOUND)
    joji_add_app(JojiBaseballEngineSFML app/visual_main.cpp)
    target_link_libraries(JojiBaseballEngineSFML PRIVATE sfml-graphics sfml-window sfml-system)
endif()

enable_testing()
add_test(NAME JojiEngineTests COMMAND JojiEngineTests)
