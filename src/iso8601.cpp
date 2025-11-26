// iso8601.cpp
//
//


#include <cstdio>

#include "iso8601.hpp"

std::string make_iso8601_from_millis(uint64_t ms)
{
    // Convert milliseconds to components
    uint64_t total_seconds = ms / 1000;
    uint32_t millis = ms % 1000;

    uint32_t seconds = total_seconds % 60;
    uint32_t minutes = (total_seconds / 60) % 60;
    uint32_t hours   = (total_seconds / 3600) % 24;
    uint32_t days    = total_seconds / 86400;

    char buffer[64];

    // Produce string like: 2025-01-28T00:12:03.512Z
    // We fake the date using "days since boot", which is perfect for experiments
    snprintf(buffer, sizeof(buffer),
             "0000-00-%02uT%02u:%02u:%02u.%03uZ",
             (unsigned)days,
             (unsigned)hours,
             (unsigned)minutes,
             (unsigned)seconds,
             (unsigned)millis
    );

    return std::string(buffer);
}