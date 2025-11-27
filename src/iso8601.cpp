// iso8601.cpp
//
//


#include <cstdio>
#include <cstdint>

#include "iso8601.hpp"

std::string make_iso8601_from_millis(uint64_t ms)
{
    // Interpret ms as milliseconds since Unix epoch (1970-01-01T00:00:00Z)

    uint64_t total_seconds = ms / 1000;
    uint32_t millis = static_cast<uint32_t>(ms % 1000);

    uint64_t days       = total_seconds / 86400;
    uint32_t sec_of_day = static_cast<uint32_t>(total_seconds % 86400);

    uint32_t hours   = sec_of_day / 3600;
    uint32_t minutes = (sec_of_day % 3600) / 60;
    uint32_t seconds = sec_of_day % 60;

    // Convert days since 1970-01-01 into year/month/day
    auto isLeap = [](uint32_t y) {
        return (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0));
    };

    uint32_t year = 1970;
    uint64_t d    = days;

    while (true) {
        uint32_t days_in_year = isLeap(year) ? 366u : 365u;
        if (d >= days_in_year) {
            d -= days_in_year;
            ++year;
        } else {
            break;
        }
    }

    static const uint8_t days_in_month[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};

    uint32_t month = 1;
    uint32_t day   = 1;

    for (int m = 0; m < 12; ++m) {
        uint32_t dim = days_in_month[m];
        if (m == 1 && isLeap(year)) {  // February in leap years
            ++dim;
        }
        if (d >= dim) {
            d -= dim;
            ++month;
        } else {
            day += static_cast<uint32_t>(d);
            break;
        }
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer),
                  "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
                  year, month, day,
                  hours, minutes, seconds, millis);

    return std::string(buffer);
}