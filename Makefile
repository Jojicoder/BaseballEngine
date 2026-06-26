CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
BUILD_DIR := build
TARGET := $(BUILD_DIR)/JojiBaseballEngine
SOURCES := app/main.cpp \
	src/AtBatTypes.cpp \
	src/AnimationPlanBuilder.cpp \
	src/AtBatEngine.cpp \
	src/BallPhysicsEngine.cpp \
	src/ContactEngine.cpp \
	src/GameEngine.cpp \
	src/GameEngineBaseRunning.cpp \
	src/JsonExporter.cpp \
	src/PitchEngine.cpp \
	src/PlayResolutionEngine.cpp \
	src/Random.cpp \
	src/RunExpectancy.cpp \
	src/SwingDecisionEngine.cpp \
	src/SwingEngine.cpp \
	src/Teams.cpp \
	src/ZoneJudge.cpp

.PHONY: all run analysis analysis-run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

analysis:
	$(MAKE) -C app analysis

analysis-run:
	$(MAKE) -C app analysis-run

clean:
	rm -rf $(BUILD_DIR)
