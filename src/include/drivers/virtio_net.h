#ifndef NUEVOOS_VIRTIO_NET_H
#define NUEVOOS_VIRTIO_NET_H

#include "types.h"

/* VirtIO vendor; legacy net 0x1000, modern transitional/modern 0x1041. */
#define VIRTIO_VENDOR_ID       0x1AF4u
#define VIRTIO_NET_DEVICE_LEGACY 0x1000u
#define VIRTIO_NET_DEVICE_MODERN 0x1041u

extern bool virtio_net_present_os;

/* Probe PCI for virtio-net; report status. Full stack not required for MVP. */
bool virtio_net_init(void);

#endif
