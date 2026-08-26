#ifndef CORDOS_E1000_H
#define CORDOS_E1000_H

#include "types.h"

bool e1000_init(void);
bool e1000_present(void);
const u8 *e1000_mac(void);
bool e1000_link_up(void);
bool e1000_send(const void *data, u16 length);
/* Returns bytes copied, 0 if the RX ring is empty. */
u16 e1000_recv(void *buf, u16 max);

#endif
