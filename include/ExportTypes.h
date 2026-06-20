#pragma once

#include <string>
#include <vector>

namespace joji {

struct TrajectoryPointSnapshot {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double timeSeconds = 0.0;
};

struct TrajectorySnapshot {
    std::string type;
    std::vector<TrajectoryPointSnapshot> points;
    double estimatedDistance = 0.0;
    double hangTime = 0.0;
    double maxHeight = 0.0;
    bool landsFair = false;
    bool crossesFence = false;
};

struct PitchSnapshot {
    int inning = 1;
    bool topHalf = true;
    std::string pitcherName;
    std::string batterName;
    std::string pitchType;
    double velocity = 0.0;
    double locationX = 0.0;
    double locationY = 0.0;
    bool isStrike = false;
    bool isInPlay = false;
};

struct AtBatSnapshot {
    int inning = 1;
    bool topHalf = true;
    std::string batterName;
    std::string pitcherName;
    std::string result;
    int pitches = 0;
    int runsScored = 0;
    int outsRecorded = 0;
    std::vector<PitchSnapshot> pitchesSeen;
};

struct PlaySnapshot {
    int inning = 1;
    bool topHalf = true;
    std::string description;
    std::string result;
    std::string batterName;
    std::string pitcherName;
    std::string fielderName;
    int runsScored = 0;
    int outsRecorded = 0;
    TrajectorySnapshot trajectory;
    std::vector<std::string> events;
};

struct LineScoreSnapshot {
    std::vector<int> away;
    std::vector<int> home;
};

struct BoxScoreSnapshot {
    int runs = 0;
    int hits = 0;
    int walks = 0;
    int atBats = 0;
    int strikeouts = 0;
    int homeRuns = 0;
    int totalBases = 0;
    int errors = 0;
    int extraBasesTaken = 0;
    int runnersThrownOutAtHome = 0;
    int sacFlyAttempts = 0;
    int sacFlySuccesses = 0;
    int homeOutfieldAssists = 0;
};

struct PlayerSnapshot {
    std::string name;
    std::string team;
    std::string position;
    int atBats = 0;
    int hits = 0;
    int doubles = 0;
    int triples = 0;
    int homeRuns = 0;
    int walks = 0;
    int strikeouts = 0;
    int rbi = 0;
    int sacFlies = 0;
    int reachedOnError = 0;
    int putouts = 0;
    int assists = 0;
    int errors = 0;
    int homeOutfieldAssists = 0;
    double avg = 0.0;
    double obp = 0.0;
    double slg = 0.0;
    double ops = 0.0;
};

struct TeamSnapshot {
    std::string name;
    int wins = 0;
    int losses = 0;
    int runsFor = 0;
    int runsAgainst = 0;
    BoxScoreSnapshot boxScore;
    std::vector<PlayerSnapshot> players;
};

struct GameResultSnapshot {
    std::string result;
    std::string winningTeam;
    std::string losingTeam;
    int awayScore = 0;
    int homeScore = 0;
    int innings = 9;
};

struct GameSnapshot {
    std::string engineVersion = "v1.0";
    std::string gameId;
    TeamSnapshot awayTeam;
    TeamSnapshot homeTeam;
    LineScoreSnapshot lineScore;
    GameResultSnapshot result;
    std::vector<AtBatSnapshot> atBats;
    std::vector<PlaySnapshot> plays;
};

} // namespace joji
