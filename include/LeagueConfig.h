#pragma once

#include <string>
#include <vector>

namespace joji {

// ── Schedule ──────────────────────────────────────────────────────────────────

struct ScheduleConfig {
    int gamesPerSeason      = 162; // total regular-season games per team
    int divisionGames       = 90;  // home + away vs each division opponent (15 home + 15 away × 3)
    int interDivisionGames  = 72;  // 12 games × 6 cross-division opponents
    int startingRotationSlots = 5;
};

// ── Playoffs ──────────────────────────────────────────────────────────────────

struct PlayoffConfig {
    int wildCardTeams       = 3;   // 3 division champs + 3 wild cards = 6 teams total
    int totalPlayoffTeams   = 6;
    // Round structure (NPB-style):
    //   Wild Card Round: #4 vs #5, #3 vs #6  (best-of-3, higher seed hosts)
    //   Semifinal:       #1 bye, #2 vs WC winner, #3 vs WC winner  (best-of-5)
    //   Final:           2 survivors  (best-of-7)
    int wildCardSeriesGames = 3;
    int semiFinalGames      = 5;
    int finalGames          = 7;
};

// ── Calendar event ────────────────────────────────────────────────────────────

struct CalendarEvent {
    std::string name;      // e.g. "キャンプ", "オープン戦", "開幕", "前半戦終了"
    std::string startDate; // YYYY-MM-DD
    std::string endDate;   // YYYY-MM-DD  (empty = single day)
    std::string notes;
};

// ── Season calendar ───────────────────────────────────────────────────────────

struct SeasonCalendar {
    int year = 2026;
    std::vector<CalendarEvent> events;
};

// ── Division ──────────────────────────────────────────────────────────────────

struct DivisionConfig {
    std::string id;        // "north" | "mid" | "south"
    std::string name;      // "North Division"
    std::vector<std::string> teamNames;
};

// ── League (top-level config) ─────────────────────────────────────────────────

struct LeagueConfig {
    std::string leagueName   = "Joji Baseball League";
    std::string leagueAbbr   = "JBL";
    std::vector<DivisionConfig> divisions;
    ScheduleConfig schedule;
    PlayoffConfig playoffs;
    SeasonCalendar calendar;
};

// ── Factory: build the JBL 2026 config ───────────────────────────────────────

inline LeagueConfig jblConfig2026() {
    LeagueConfig cfg;
    cfg.leagueName = "Joji Baseball League";
    cfg.leagueAbbr = "JBL";

    cfg.divisions = {
        {
            "north", "North Division",
            {
                "Newark Knights",
                "Queens Titans",
                "Brooklyn Hammers",
                "Bronx Wolves",
                "Harlem Eagles",
                "Staten Island Foxes"
            }
        },
        {
            "mid", "Mid Division",
            {
                "Fishtown Ferals",
                "Kensington Iron",
                "Germantown Colonials",
                "Manayunk Runners",
                "Fairmount Rams",
                "South Philly Stallions"
            }
        },
        {
            "south", "South Division",
            {
                "Georgetown Ravens",
                "Capitol Hill Senators",
                "Anacostia Kings",
                "Alexandria Cannons",
                "Bethesda Blaze",
                "Silver Spring Ghosts"
            }
        }
    };

    cfg.schedule = {
        162, // gamesPerSeason
        90,  // divisionGames  (18 vs each of 5 opponents = 90)
        72,  // interDivisionGames (6 series × 6 games × 2 divisions = 72)
        5
    };

    cfg.playoffs = {
        3,  // wildCardTeams
        6,  // totalPlayoffTeams
        3,  // wildCardSeriesGames
        5,  // semiFinalGames
        7   // finalGames
    };

    cfg.calendar = {
        2026,
        {
            {"キャンプ",           "2026-02-01", "2026-03-05", "Spring training begins"},
            {"オープン戦",         "2026-03-10", "2026-03-27", "Exhibition games"},
            {"開幕",               "2026-04-03", "",           "Opening Day"},
            {"前半戦",             "2026-04-03", "2026-07-10", "First half"},
            {"前半戦終了",         "2026-07-10", "",           "First half ends"},
            {"オールスター",       "2026-07-14", "2026-07-15", "All-Star Game"},
            {"後半戦開始",         "2026-07-17", "",           "Second half begins"},
            {"後半戦",             "2026-07-17", "2026-09-25", "Second half"},
            {"レギュラーシーズン終了", "2026-09-25", "",        "Regular season ends"},
            {"プレーオフ",         "2026-10-01", "2026-10-20", "Playoffs (wildcard → final)"},
            {"JBL日本シリーズ",    "2026-10-22", "2026-10-31", "JBL Championship Series"},
            {"ドラフト",           "2026-11-07", "",           "Amateur draft"},
            {"FA解禁",             "2026-11-15", "",           "Free agency opens"},
            {"契約更改",           "2026-11-15", "2026-12-31", "Contract negotiations"},
        }
    };

    return cfg;
}

} // namespace joji
