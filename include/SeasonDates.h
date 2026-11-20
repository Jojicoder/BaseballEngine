#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace joji {

// ── Season calendar ───────────────────────────────────────────────────────────
//
// Where the regular season sits on the calendar.  The schedule itself is built
// from "playing day" indices (0 .. playingDays()-1); this maps them to dates,
// skipping the days with no games (the All-Star break).
//
// 2026 is the legacy layout and must never change: published 2026 data (and the
// incremental export that resumes it) depends on these exact dates.
// 2027 follows the real MLB calendar published by MLB in July 2026:
//   Opening Day Thu Mar 25, All-Star Game Tue Jul 13 (no games Jul 12-14),
//   second half starts Thu Jul 15, last day of the regular season Sun Sep 26.
// Any other year falls back to the legacy layout (Apr 1 + 178 days).

struct DateRange {
    std::string first;  // YYYY-MM-DD, inclusive
    std::string last;   // YYYY-MM-DD, inclusive
};

namespace seasondates_detail {

// Days since 1970-01-01 for a proleptic Gregorian date (no time zones, no DST).
inline long toSerial(int y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

inline long parse(const std::string& date) {
    return toSerial(std::stoi(date.substr(0, 4)), std::stoi(date.substr(5, 2)), std::stoi(date.substr(8, 2)));
}

inline std::string format(long serial) {
    serial += 719468;
    const long era = (serial >= 0 ? serial : serial - 146096) / 146097;
    const long doe = serial - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    const long d = doy - (153 * mp + 2) / 5 + 1;
    const long m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) ++y;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04ld-%02ld-%02ld", y, m, d);
    return buf;
}

} // namespace seasondates_detail

struct SeasonDates {
    std::string openingDay;
    std::string lastDay;
    std::vector<DateRange> noGameRanges;

    bool isNoGameDay(long serial) const {
        for (const auto& range : noGameRanges) {
            if (serial >= seasondates_detail::parse(range.first) && serial <= seasondates_detail::parse(range.last)) {
                return true;
            }
        }
        return false;
    }

    // Number of days on which games can be played.
    int playingDays() const {
        const long open = seasondates_detail::parse(openingDay);
        const long last = seasondates_detail::parse(lastDay);
        int count = 0;
        for (long day = open; day <= last; ++day) {
            if (!isNoGameDay(day)) ++count;
        }
        return count;
    }

    // Date of the Nth playing day (0-based).
    std::string dateForPlayingDay(int index) const {
        long day = seasondates_detail::parse(openingDay);
        int seen = 0;
        for (;; ++day) {
            if (isNoGameDay(day)) continue;
            if (seen == index) return seasondates_detail::format(day);
            ++seen;
        }
    }
};

inline SeasonDates seasonDates(int season) {
    SeasonDates dates;
    if (season == 2027) {
        dates.openingDay = "2027-03-25";
        dates.lastDay = "2027-09-26";
        dates.noGameRanges = {{"2027-07-12", "2027-07-14"}};  // Home Run Derby, All-Star Game, travel
        return dates;
    }
    // Legacy layout (used for 2026): Apr 1 through Sep 25, no breaks.
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-04-01", season);
    dates.openingDay = buf;
    dates.lastDay = seasondates_detail::format(seasondates_detail::parse(dates.openingDay) + 177);
    return dates;
}

} // namespace joji
