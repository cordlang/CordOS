#ifndef CORDOS_PIC_H
#define CORDOS_PIC_H

#include "types.h"

void pic_remap(u8 offset1, u8 offset2);
void pic_eoi(u8 irq);
void pic_set_mask(u8 irq);
void pic_clear_mask(u8 irq);
void pic_mask_all(void);

#endif
