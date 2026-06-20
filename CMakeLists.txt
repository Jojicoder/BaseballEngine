cmake_minimum_required(VERSION 3.16)

project(JojiBaseballEngine
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(JojiBaseballEngine
    app/main.cpp
    src/AtBatTypes.cpp
    src/AnimationPlanBuilder.cpp
    src/AtBatEngine.cpp
    src/BallPhysicsEngine.cpp
    src/ContactEngine.cpp
    src/GameEngine.cpp
    src/PitchEngine.cpp
    src/PlayResolutionEngine.cpp
    src/Random.cpp
    src/SwingDecisionEngine.cpp
    src/SwingEngine.cpp
    src/Teams.cpp
    src/ZoneJudge.cpp
)

target_include_directories(JojiBaseballEngine PRIVATE include)

if (MSVC)
    target_compile_options(JojiBaseballEngine PRIVATE /W4)
else()
    target_compile_options(JojiBaseballEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()
