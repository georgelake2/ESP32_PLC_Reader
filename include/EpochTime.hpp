// EpochTime.hpp
// George Lake
// Fall 2025
//
// Convert PLC DateTime DINT[7] -> UTC epoch milliseconds (ESP-side)


#pragma once
#include <array>
#include <cstdint>

namespace EpochTime {
  struct PlcDateTime { int32_t year, month, day, hour, minute, second, usec; };

  // Construct PlcDateTime from DateTime[0..6] DINTs.
  PlcDateTime fromArray(const std::array<int32_t,7>& a);

  // Convert to epoch ms, with optional timezone minutes offset (PLC local→UTC).
  int64_t toEpochMs(const PlcDateTime& t, int32_t tzOffsetMinutes);

  // Milliseconds since ESP boot (for simple timestamps).
  int64_t espNowMs();
}
