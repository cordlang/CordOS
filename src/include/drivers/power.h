#ifndef NUEVOOS_POWER_H
#define NUEVOOS_POWER_H

#include "types.h"

/* Ask the machine to power off (QEMU, VirtualBox, ACPI PM). Does not return
 * if the hypervisor honors the request. */
void machine_power_off(void);

/* Charge 0–100 and whether the machine is on AC. VM hosts report full + AC. */
u32 power_battery_percent(void);
bool power_on_ac(void);

#endif
