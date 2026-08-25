#ifndef NUEVOOS_IPC_H
#define NUEVOOS_IPC_H

#include "types.h"

#define PIPE_BUF_SIZE 256
#define PIPE_MAX      8

/* In-kernel pipe stub: create / read / write. Returns -1 on error. */
i32 pipe_create(void);
ssize_t pipe_read(i32 pipe_id, void *buf, size_t len);
ssize_t pipe_write(i32 pipe_id, const void *buf, size_t len);
void pipe_close(i32 pipe_id);

#endif
