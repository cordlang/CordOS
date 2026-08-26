#ifndef CORDOS_NIC_H
#define CORDOS_NIC_H

#include "types.h"

/* Probe e1000 then PCnet. True if a usable TX/RX NIC is up. */
bool nic_init(void);
bool nic_present(void);
const u8 *nic_mac(void);
bool nic_send(const void *data, u16 length);
u16 nic_recv(void *buf, u16 max);

#endif
