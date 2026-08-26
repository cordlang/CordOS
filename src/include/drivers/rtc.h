#ifndef CORDOS_RTC_H
#define CORDOS_RTC_H

#include "types.h"

struct rtc_time {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
};

/* Read a stable CMOS time snapshot and decode it to binary values. */
bool rtc_read(struct rtc_time *out);

#endif
