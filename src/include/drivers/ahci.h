#ifndef CORDOS_AHCI_H
#define CORDOS_AHCI_H

/* Probe PCI AHCI (class 01:06 prog-if 01). Registers each SATA HDD with blk. */
void ahci_init(void);

#endif
