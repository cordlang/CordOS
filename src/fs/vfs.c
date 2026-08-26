#include "vfs.h"
#include "ahci.h"
#include "ata.h"
#include "blk.h"
#include "initrd.h"
#include "nosfs.h"
#include "persist.h"
#include "serial.h"
#include "string.h"
#include "task.h"
#include "vga.h"

#define VFS_MAX_FD   8
#define VFS_SRC_RAM  0
#define VFS_SRC_DISK 1

struct vfs_file {
    int used;
    int src;
    u32 owner_pid;
    u32 data_off;
    u32 size;
    u32 pos;
    char name[32];
};

struct list_ctx {
    void (*cb)(const char *name, u32 size, void *arg);
    void *arg;
    char seen[NOSFS_MAX_FILES + 8][32];
    u32 nseen;
};

static struct vfs_file s_fds[VFS_MAX_FD];
static int s_ready;
static int s_disk;

static const char *vfs_basename(const char *path)
{
    if (path == NULL) {
        return "";
    }
    while (*path == '/') {
        ++path;
    }
    return path;
}

static int path_is_root(const char *path)
{
    const char *p = path;

    if (p == NULL) {
        return 1;
    }
    while (*p == '/') {
        ++p;
    }
    return *p == '\0';
}

static void copy_name32(char *dst, const char *src)
{
    u32 i;

    memset(dst, 0, 32);
    for (i = 0; i < 31u && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
}

static int disk_selftest(void)
{
    static const char payload[] = "CordOS nosfs writeback\n";
    char buf[40];
    int n;

    if (nosfs_disk_put("p6test", payload, sizeof(payload) - 1u) < 0) {
        return -1;
    }
    if (nosfs_disk_reload() < 0) {
        return -1;
    }
    n = nosfs_disk_read("p6test", 0, buf, sizeof(payload) - 1u);
    if (n != (int)(sizeof(payload) - 1u)) {
        return -1;
    }
    if (memcmp(buf, payload, sizeof(payload) - 1u) != 0) {
        return -1;
    }
    return 0;
}

void phase6_init(void)
{
    u32 i;
    int ram_ok;

    s_ready = 0;
    s_disk = 0;
    for (i = 0; i < VFS_MAX_FD; ++i) {
        s_fds[i].used = 0;
    }

    blk_init();
    ata_init();
    ahci_init();
    ram_ok = (nosfs_mount(initrd_blob, initrd_blob_size) == 0);

    if (blk_count() == 0u) {
        serial_write("phase6: no writable HDD/SSD (need AHCI SATA or IDE)\n");
    } else if (nosfs_disk_mount() == 0) {
        s_disk = 1;
        if (disk_selftest() == 0) {
            serial_write("phase6: disk writeback OK\n");
        } else {
            serial_write("phase6: disk writeback FAIL\n");
        }
    }

    if (s_disk) {
        s_ready = 1;
        serial_write("phase6: root=disk (initrd overlay RO)\n");
    } else if (ram_ok) {
        s_ready = 1;
        serial_write("phase6: root=initrd\n");
    } else {
        serial_write("phase6: no root\n");
        persist_init();
        return;
    }

    persist_init();
}

int vfs_open(const char *path)
{
    const char *name;
    u32 off;
    u32 size;
    int fd;
    int src;

    if (!s_ready || path == NULL) {
        return -1;
    }

    name = vfs_basename(path);
    if (*name == '\0') {
        return -1;
    }

    src = VFS_SRC_RAM;
    off = 0;
    size = 0;
    if (s_disk && nosfs_disk_lookup(name, &size) == 0) {
        src = VFS_SRC_DISK;
    } else if (nosfs_lookup(name, &off, &size) == 0) {
        src = VFS_SRC_RAM;
    } else {
        return -1;
    }

    for (fd = 0; fd < VFS_MAX_FD; ++fd) {
        if (!s_fds[fd].used) {
            s_fds[fd].used = 1;
            s_fds[fd].src = src;
            s_fds[fd].owner_pid = current_task_os != NULL
                ? current_task_os->pid
                : 0;
            s_fds[fd].data_off = off;
            s_fds[fd].size = size;
            s_fds[fd].pos = 0;
            copy_name32(s_fds[fd].name, name);
            return fd;
        }
    }
    return -1;
}

int vfs_create(const char *path)
{
    const char *name;

    if (!s_ready || !s_disk || path == NULL) {
        return -1;
    }
    name = vfs_basename(path);
    if (*name == '\0') {
        return -1;
    }
    return nosfs_disk_put(name, NULL, 0);
}

ssize_t vfs_read(int fd, void *buf, size_t len)
{
    struct vfs_file *f;
    u32 remain;
    u32 n;
    const u8 *src;
    u8 *dst;
    int got;

    if (!s_ready || fd < 0 || fd >= VFS_MAX_FD || buf == NULL) {
        return -1;
    }

    f = &s_fds[fd];
    if (!f->used) {
        return -1;
    }

    if (f->pos >= f->size) {
        return 0;
    }

    remain = f->size - f->pos;
    n = (u32)len;
    if (n > remain) {
        n = remain;
    }

    if (f->src == VFS_SRC_DISK) {
        got = nosfs_disk_read(f->name, f->pos, buf, n);
        if (got < 0) {
            return -1;
        }
        f->pos += (u32)got;
        return (ssize_t)got;
    }

    src = nosfs_data() + f->data_off + f->pos;
    dst = (u8 *)buf;
    memcpy(dst, src, n);
    f->pos += n;
    return (ssize_t)n;
}

ssize_t vfs_write(int fd, const void *buf, size_t len)
{
    struct vfs_file *f;
    u32 n;

    if (!s_ready || fd < 0 || fd >= VFS_MAX_FD || buf == NULL) {
        return -1;
    }

    f = &s_fds[fd];
    if (!f->used) {
        return -1;
    }
    if (!s_disk) {
        return -1;
    }

    n = (u32)len;
    /* Writes at pos 0 replace the whole file (simple persist / cat >). */
    if (f->pos != 0) {
        return -1;
    }
    if (nosfs_disk_put(f->name, buf, n) < 0) {
        return -1;
    }
    f->src = VFS_SRC_DISK;
    f->size = n;
    f->pos = n;
    return (ssize_t)n;
}

int vfs_close(int fd)
{
    if (!s_ready || fd < 0 || fd >= VFS_MAX_FD) {
        return -1;
    }
    if (!s_fds[fd].used) {
        return -1;
    }
    s_fds[fd].used = 0;
    s_fds[fd].owner_pid = 0;
    return 0;
}

void vfs_close_task(u32 pid)
{
    int fd;

    /* Idle / kernel (pid 0) shares the global table with the desktop. */
    if (pid == 0) {
        return;
    }
    for (fd = 0; fd < VFS_MAX_FD; ++fd) {
        if (s_fds[fd].used && s_fds[fd].owner_pid == pid) {
            s_fds[fd].used = 0;
            s_fds[fd].owner_pid = 0;
        }
    }
}

static int seen_has(struct list_ctx *ctx, const char *name)
{
    u32 i;
    u32 j;

    for (i = 0; i < ctx->nseen; ++i) {
        for (j = 0; j < 32u; ++j) {
            if (ctx->seen[i][j] != name[j]) {
                break;
            }
            if (name[j] == '\0') {
                return 1;
            }
        }
        if (j == 32u) {
            return 1;
        }
    }
    return 0;
}

static void seen_add(struct list_ctx *ctx, const char *name)
{
    if (ctx->nseen >= (u32)(sizeof(ctx->seen) / sizeof(ctx->seen[0]))) {
        return;
    }
    copy_name32(ctx->seen[ctx->nseen], name);
    ctx->nseen++;
}

static void disk_list_cb(const char *name, u32 size, void *arg)
{
    struct list_ctx *ctx = (struct list_ctx *)arg;

    seen_add(ctx, name);
    ctx->cb(name, size, ctx->arg);
}

static void ram_list_cb(const char *name, u32 size, void *arg)
{
    struct list_ctx *ctx = (struct list_ctx *)arg;

    if (seen_has(ctx, name)) {
        return;
    }
    ctx->cb(name, size, ctx->arg);
}

int vfs_list(const char *path,
             void (*cb)(const char *name, u32 size, void *arg),
             void *arg)
{
    static struct list_ctx ctx;
    int n = 0;
    int r;

    if (!s_ready || cb == NULL) {
        return -1;
    }
    if (!path_is_root(path)) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.cb = cb;
    ctx.arg = arg;

    if (s_disk) {
        r = nosfs_disk_list(disk_list_cb, &ctx);
        if (r < 0) {
            return -1;
        }
        n += r;
    }
    r = nosfs_list(ram_list_cb, &ctx);
    if (r >= 0) {
        n += r;
    }
    return n;
}

static void ls_print_cb(const char *name, u32 size, void *arg)
{
    (void)size;
    (void)arg;
    vga_print(name);
    vga_print("\n");
}

int vfs_ls(const char *path)
{
    if (!s_ready) {
        return -1;
    }
    if (vfs_list(path, ls_print_cb, NULL) < 0) {
        return -1;
    }
    return 0;
}
