#ifndef NUEVOOS_TIME_H
#define NUEVOOS_TIME_H

#include "types.h"

extern volatile u32 ticks_os;
extern volatile u32 hz_os;

void time_init(u32 frequency_hz);
u32 time_ticks(void);
u32 time_uptime_ms(void);

/* TSC (rdtsc). Calibrate after IRQ0 is running. */
u64 time_tsc(void);
void time_tsc_calibrate(void);
u32 time_us_since(u64 tsc0);

#endif
