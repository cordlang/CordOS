#ifndef NUEVOOS_PAGE_FAULT_H
#define NUEVOOS_PAGE_FAULT_H

#include "isr.h"

void page_fault_handler(struct interrupt_frame *frame);

#endif
