// EpochTime.cpp
// George Lake
// Fall 2025
//
// PLC time -> epoch time conversion


#include "EpochTime.hpp"
#include "esp_timer.h"

namespace {
  inline bool isLeap(int32_t y){ return (y%4==0) && ((y%100!=0) || (y%400==0)); }
  int32_t daysBeforeYear(int32_t y){
    int64_t y64 = y - 1970;
    int64_t leaps = ((y-1)/4 - (1970-1)/4) - ((y-1)/100 - (1970-1)/100) + ((y-1)/400 - (1970-1)/400);
    return (int32_t)(y64*365 + leaps);
  }
  int32_t daysBeforeMonth(int32_t y, int32_t m){
    static const int md[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    int32_t d=0;
    for(int i=1;i<m;++i) d += (i==2 && isLeap(y)) ? 29 : md[i-1];
    return d;
  }
}

namespace EpochTime {
  PlcDateTime fromArray(const std::array<int32_t,7>& a) {
    return PlcDateTime{a[0],a[1],a[2],a[3],a[4],a[5],a[6]};
  }

  int64_t toEpochMs(const PlcDateTime& t, int32_t tzOffsetMinutes) {
    if (t.year < 2000 || t.month < 1 || t.month > 12 || t.day < 1) return -1;
    int64_t days = (int64_t)daysBeforeYear(t.year) + (int64_t)daysBeforeMonth(t.year, t.month) + (t.day - 1);
    int64_t ms   = days*86400000LL + (int64_t)t.hour*3600000LL + (int64_t)t.minute*60000LL
                 + (int64_t)t.second*1000LL + (int64_t)(t.usec/1000);
    ms -= (int64_t)tzOffsetMinutes * 60000LL;
    return ms;
  }

  int64_t espNowMs(){ return esp_timer_get_time() / 1000; }
}
