#ifndef LIBNOS_SYSCALL_H
#define LIBNOS_SYSCALL_H

/* Userland prototypes — pair with user/libnos/syscall.S
 * ABI: int $0x80, rax=num, rdi/rsi/rdx=args, return in rax. */

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_READ   2
#define SYS_YIELD  3
#define SYS_GETPID 4
#define SYS_MMAP   5
#define SYS_OPEN   6
#define SYS_CLOSE  7
#define SYS_SPAWN  8
#define SYS_EXEC   9

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

long nos_exit(long code);
long nos_write(long fd, const void *buf, unsigned long len);
long nos_read(long fd, void *buf, unsigned long len);
long nos_yield(void);
long nos_getpid(void);
long nos_mmap(void *hint, unsigned long len, long prot);
long nos_open(const char *path);
long nos_close(long fd);
long nos_spawn(const char *path);
long nos_exec(const char *path);

#endif
