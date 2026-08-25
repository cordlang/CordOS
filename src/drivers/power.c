#include "power.h"
#include "io.h"
#include "serial.h"
#include "time.h"

static void power_wait_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("pause");
    }
}

u32 power_battery_percent(void)
{
    /* No ACPI battery yet; a hosted VM is treated as full. */
    return 100u;
}

bool power_on_ac(void)
{
    return true;
}

void machine_power_off(void)
{
    serial_write("power: shutting down\n");

    /* Let the "Apagando" frame reach the screen. */
    power_wait_ms(250);

    __asm__ volatile("cli");

    /* QEMU i440fx / q35 ACPI PM1a_CNT (SLP_EN). */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);

    /* VirtualBox PIIX ACPI: PM1a_CNT at 0x4004, SLP_TYP=S5 | SLP_EN. */
    outw(0x4004, 0x3400);

    /* Cloud Hypervisor / some ACPI PM bases. */
    outw(0x600, 0x34);

    /* If the hypervisor ignored us, stay halted. */
    for (;;) {
        __asm__ volatile("hlt");
    }
}
