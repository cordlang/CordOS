#ifndef CORDOS_PCNET_H
#define CORDOS_PCNET_H

#include "types.h"

bool pcnet_init(void);
bool pcnet_present(void);
const u8 *pcnet_mac(void);
bool pcnet_send(const void *data, u16 length);
u16 pcnet_recv(void *buf, u16 max);

#endif
