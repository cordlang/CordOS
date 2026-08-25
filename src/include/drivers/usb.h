#ifndef NUEVOOS_USB_H
#define NUEVOOS_USB_H

#include "types.h"

struct usb_device {
    u8 addr;
    u16 vendor;
    u16 product;
    u8 ep_in;  /* bulk IN endpoint number (1–15) */
    u8 ep_out; /* bulk OUT endpoint number */
    u16 maxpkt;
    bool high_speed;
};

bool usb_ehci_init(void);
bool usb_ehci_present(void);
u32 usb_device_count(void);
const struct usb_device *usb_device_get(u32 index);

/* Control transfer. data may be NULL when length is 0. Returns 0 on success. */
int usb_ctrl(u8 addr, u8 reqtype, u8 req, u16 value, u16 index, void *data,
             u16 length);

/* Bulk transfer. Returns bytes transferred, or < 0 on error. */
int usb_bulk(u8 addr, u8 ep, bool in, void *data, u16 length);

#endif
