#include "rtc.h"
#include "io.h"

static u8 rtc_read_reg(u8 reg)
{
    outb(0x70, (u8)(reg | 0x80u));
    return inb(0x71);
}

static u8 rtc_decode(u8 value, bool binary)
{
    if (binary) {
        return value;
    }
    return (u8)(((value >> 4) * 10u) + (value & 0x0Fu));
}

static u8 rtc_decode_hour(u8 value, bool binary, bool mode24)
{
    bool pm = (value & 0x80u) != 0;
    u8 hour = rtc_decode((u8)(value & 0x7Fu), binary);

    if (mode24) {
        return hour;
    }
    if (hour == 12u) {
        hour = 0;
    }
    if (pm) {
        hour = (u8)(hour + 12u);
    }
    return hour;
}

bool rtc_read(struct rtc_time *out)
{
    u32 attempt;

    if (out == NULL) {
        return false;
    }

    for (attempt = 0; attempt < 8u; ++attempt) {
        u8 status_b;
        u8 second_raw;
        u8 minute_raw;
        u8 hour_raw;
        u8 day_raw;
        u8 month_raw;
        u8 year_raw;
        u8 century_raw;
        bool binary;
        bool mode24;
        u8 second;
        u8 minute;
        u8 hour;
        u8 day;
        u8 month;
        u8 year;
        u8 century;
        u16 full_year;

        /* UIP means the RTC is between two calendar snapshots. */
        if ((rtc_read_reg(0x0A) & 0x80u) != 0) {
            continue;
        }
        status_b = rtc_read_reg(0x0B);
        binary = (status_b & 0x04u) != 0;
        mode24 = (status_b & 0x02u) != 0;
        second_raw = rtc_read_reg(0x00);
        minute_raw = rtc_read_reg(0x02);
        hour_raw = rtc_read_reg(0x04);
        day_raw = rtc_read_reg(0x07);
        month_raw = rtc_read_reg(0x08);
        year_raw = rtc_read_reg(0x09);
        century_raw = rtc_read_reg(0x32);
        if ((rtc_read_reg(0x0A) & 0x80u) != 0 ||
            rtc_read_reg(0x00) != second_raw) {
            continue;
        }

        second = rtc_decode(second_raw, binary);
        minute = rtc_decode(minute_raw, binary);
        hour = rtc_decode_hour(hour_raw, binary, mode24);
        day = rtc_decode(day_raw, binary);
        month = rtc_decode(month_raw, binary);
        year = rtc_decode(year_raw, binary);
        century = rtc_decode(century_raw, binary);

        if (second > 59u || minute > 59u || hour > 23u ||
            day < 1u || day > 31u || month < 1u || month > 12u) {
            continue;
        }
        full_year = (century >= 19u && century <= 99u)
                        ? (u16)((u16)century * 100u + (u16)year)
                        : (u16)((u16)2000u + (u16)year);
        out->second = second;
        out->minute = minute;
        out->hour = hour;
        out->day = day;
        out->month = month;
        out->year = full_year;
        return true;
    }
    return false;
}
