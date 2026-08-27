#ifndef CORDOS_VFS_H
#define CORDOS_VFS_H

#include "types.h"

int vfs_open(const char *path);
ssize_t vfs_read(int fd, void *buf, size_t len);
ssize_t vfs_write(int fd, const void *buf, size_t len);
int vfs_create(const char *path);
int vfs_close(int fd);
/* Size of an open file. Console fds 0/1 are not in this table. */
int vfs_size(int fd, u32 *out);

/* Close FDs opened by pid. pid 0 (idle/kernel) is ignored. */
void vfs_close_task(u32 pid);

/* Print directory entries (basenames). Returns 0 or -1. */
int vfs_ls(const char *path);

/* Callback list of the overlay root (disk shadows initrd). */
int vfs_list(const char *path,
             void (*cb)(const char *name, u32 size, void *arg),
             void *arg);

/* Probe ATA, mount NOSF disk if present, else initrd. Then persist_init. */
void phase6_init(void);

#endif
