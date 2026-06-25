CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -I../include
SEASON_CXXFLAGS := -std=c++17 -O3 -march=native -Wall -Wextra -Wpedantic -I../include -pthread
SFML := /opt/homebrew/opt/sfml
SFML_CXXFLAGS := $(CXXFLAGS) -I$(SFML)/include
SFML_LDFLAGS := -L$(SFML)/lib -lsfml-graphics -lsfml-window -lsfml-system
BUILD_DIR := ../build
TARGET := $(BUILD_DIR)/JojiBaseballEngine
SFML_TARGET := $(BUILD_DIR)/JojiBaseballEngineSFML
ATBAT_TARGET := $(BUILD_DIR)/JojiAtBatDebug
TEST_TARGET := $(BUILD_DIR)/JojiEngineTests
STATS_TARGET := $(BUILD_DIR)/JojiStatsRunner
ANALYSIS_TARGET := $(BUILD_DIR)/JojiAnalysisRunner
SEASON_TARGET := $(BUILD_DIR)/JojiSeasonRunner
LEAGUE_TARGET := $(BUILD_DIR)/JojiLeagueRunner
ENGINE_SOURCES := \
	../src/AtBatTypes.cpp \
	../src/AnimationPlanBuilder.cpp \
	../src/AtBatEngine.cpp \
	../src/BallPhysicsEngine.cpp \
	../src/ContactEngine.cpp \
	../src/GameEngine.cpp \
	../src/JsonExporter.cpp \
	../src/PitchEngine.cpp \
	../src/PlayResolutionEngine.cpp \
	../src/Random.cpp \
	../src/SwingDecisionEngine.cpp \
	../src/SwingEngine.cpp \
	../src/Teams.cpp \
	../src/ZoneJudge.cpp
SOURCES := main.cpp \
	$(ENGINE_SOURCES)
SFML_SOURCES := visual_main.cpp \
	$(ENGINE_SOURCES)
ATBAT_SOURCES := atbat_debug.cpp \
	$(ENGINE_SOURCES)
TEST_SOURCES := engine_tests.cpp \
	$(ENGINE_SOURCES)
STATS_SOURCES := stats_runner.cpp \
	$(ENGINE_SOURCES)
ANALYSIS_SOURCES := analysis_runner.cpp \
	$(ENGINE_SOURCES)
SEASON_SOURCES := season_runner.cpp \
	$(ENGINE_SOURCES)
LEAGUE_SOURCES := league_runner.cpp \
	$(ENGINE_SOURCES)
PITCHCHECK_TARGET := $(BUILD_DIR)/JojiPitchCheck
PITCHCHECK_SOURCES := pitch_scoring_check.cpp \
	$(ENGINE_SOURCES)

.PHONY: all run sfml sfml-run atbat atbat-run test stats stats-run analysis analysis-run season season-run league league-run pitchcheck clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

sfml: $(SFML_TARGET)

$(SFML_TARGET): $(SFML_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(SFML_CXXFLAGS) $(SFML_SOURCES) -o $(SFML_TARGET) $(SFML_LDFLAGS)

sfml-run: sfml
	./$(SFML_TARGET)

atbat: $(ATBAT_TARGET)

$(ATBAT_TARGET): $(ATBAT_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(ATBAT_SOURCES) -o $(ATBAT_TARGET)

atbat-run: atbat
	./$(ATBAT_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET)

stats: $(STATS_TARGET)

$(STATS_TARGET): $(STATS_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(STATS_SOURCES) -o $(STATS_TARGET)

stats-run: stats
	./$(STATS_TARGET)

analysis: $(ANALYSIS_TARGET)

$(ANALYSIS_TARGET): $(ANALYSIS_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(ANALYSIS_SOURCES) -o $(ANALYSIS_TARGET)

analysis-run: analysis
	./$(ANALYSIS_TARGET) $(ARGS)

season: $(SEASON_TARGET)

$(SEASON_TARGET): $(SEASON_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(SEASON_CXXFLAGS) $(SEASON_SOURCES) -o $(SEASON_TARGET)

season-run: season
	./$(SEASON_TARGET) $(ARGS)

league: $(LEAGUE_TARGET)

$(LEAGUE_TARGET): $(LEAGUE_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LEAGUE_SOURCES) -o $(LEAGUE_TARGET)

league-run: league
	./$(LEAGUE_TARGET) $(ARGS)

pitchcheck: $(PITCHCHECK_TARGET)
	./$(PITCHCHECK_TARGET)

$(PITCHCHECK_TARGET): $(PITCHCHECK_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PITCHCHECK_SOURCES) -o $(PITCHCHECK_TARGET)

clean:
	rm -rf $(BUILD_DIR)
