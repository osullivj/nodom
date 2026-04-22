// utility functions
#pragma once
#include "nd_types.hpp"
#include "static_strings.hpp"

inline std::ostream& operator<<(std::ostream& os, const YMD& date) {
    os << "[" << date[Year] << "," << date[Month] << "," << date[Day] << "]";
    return os;
}

inline bool IsLeapYear(int year) {
    if ((year % 400) == 0)
        return true;
    if ((year % 4 == 0) && ((year % 100) != 0))
        return true;
    return false;
}

int MonthDayCount(int month, int year) {
    if (month == 2) {
        return IsLeapYear(year) ? 29 : 28;
    }
    assert(month < 13);
    return Static::month_day_count[month - 1];
}

inline int WeekDay(int day, int month, int year) {
    if ((month == 1) || (month == 2)) {
        month += 12;
        year -= 1;
    }
    int h = (day    // Zeller's congruence
        + static_cast<int>(std::floor((13 * (month + 1)) / 5.0))
        + year
        + static_cast<int>(std::floor(year / 4.0))
        - static_cast<int>(std::floor(year / 100.0))
        + static_cast<int>(std::floor(year / 400.0))) % 7;
    return static_cast<int>(std::floor(((h + 5) % 7) + 1));
}

inline int MonthWeekCount(int month, int year) {
    int day_count = MonthDayCount(month, year);
    int day_one = WeekDay(1, month, year);
    return static_cast<int>(std::ceil((day_count + day_one - 1) / 7.0));
}

inline void DecrementMonth(YMD date) {
    // special case for Jan: previous month is Dec in prev year
    if (date[Month] == 1) {
        date[Year] = date[Year] - 1;
        date[Month] = 12;
        date[Day] = std::min(date[Day], MonthDayCount(12, date[Year] - 1));
        return;
    }
    date[Day] = std::min(date[Day], MonthDayCount(date[Month] - 1, date[Year]));
    date[Month] = date[Month] - 1;
}

inline void IncrementMonth(YMD date) {
    if (date[Month] == 12) {
        date[Year] = date[Year] + 1;
        date[Month] = 1;
        date[Day] = std::min(date[Day], MonthDayCount(1, date[Year] + 1));
        return;
    }
    date[Month] = date[Month] + 1;
    date[Day] = std::min(date[Day], MonthDayCount(date[Month] + 1, date[Year]));
}

inline std::ostream& operator<<(std::ostream& os, const WEEK& dates) {
    os << "["
        << dates[Mon] << ","
        << dates[Tues] << ","
        << dates[Weds] << ","
        << dates[Thurs] << ","
        << dates[Fri] << ","
        << dates[Sat] << ","
        << dates[Sun] << "]";

    return os;
}

inline void WeekDates(int week, int day_one, int days_in_month, WEEK& wk) {
    std::fill_n(wk.begin(), wk.size(), 0);
    int start_of_week = 7 * (week - 1) + 2 - day_one;
    if (start_of_week >= 1) {
        wk[Mon] = start_of_week;
    }
    for (int i = Tues; i < EndWeekDay; ++i) {
        int date = day_one + i;
        if ((date >= 1) && (date <= days_in_month)) {
            wk[i] = date;
        }
    }
}
