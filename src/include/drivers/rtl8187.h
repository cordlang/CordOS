#ifndef CORDOS_RTL8187_H
#define CORDOS_RTL8187_H

#include "types.h"

bool rtl8187_init(void);
bool rtl8187_present(void);
u32 rtl8187_scan(void);
bool rtl8187_associate(const char *ssid, const char *pass, u8 sec);

#endif
