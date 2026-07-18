#include "timestamp.hpp"

#include <ctime>

namespace backtest {

HighResTimestamp::Components HighResTimestamp::decompose(Timestamp ts) {
    Components c{};
    time_t seconds = ts / NANOSECONDS_PER_SECOND;
    int64_t nanos = ts % NANOSECONDS_PER_SECOND;

    std::tm* tm = std::localtime(&seconds);
    c.year = tm->tm_year + 1900;
    c.month = tm->tm_mon + 1;
    c.day = tm->tm_mday;
    c.hour = tm->tm_hour;
    c.minute = tm->tm_min;
    c.second = tm->tm_sec;
    c.millisecond = static_cast<int>(nanos / NANOSECONDS_PER_MILLISECOND);
    c.microsecond = static_cast<int>((nanos % NANOSECONDS_PER_MILLISECOND) / NANOSECONDS_PER_MICROSECOND);
    c.nanosecond = static_cast<int>(nanos % NANOSECONDS_PER_MICROSECOND);

    return c;
}

Timestamp HighResTimestamp::from_components(const Components& c) {
    std::tm tm{};
    tm.tm_year = c.year - 1900;
    tm.tm_mon = c.month - 1;
    tm.tm_mday = c.day;
    tm.tm_hour = c.hour;
    tm.tm_min = c.minute;
    tm.tm_sec = c.second;

    auto time_t_val = std::mktime(&tm);
    int64_t nanos = static_cast<int64_t>(c.millisecond) * NANOSECONDS_PER_MILLISECOND +
                    static_cast<int64_t>(c.microsecond) * NANOSECONDS_PER_MICROSECOND +
                    c.nanosecond;

    return static_cast<Timestamp>(time_t_val) * NANOSECONDS_PER_SECOND + nanos;
}

}  // namespace backtest
