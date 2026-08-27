#ifndef CORDOS_PHASE9_H
#define CORDOS_PHASE9_H

/* Serial + PCI enum + virtio-net detect + pipe stub self-test. */
void phase9_init(void);

/* Boot self-tests. Panics if a check fails. */
void kselftest_run(void);

#endif
