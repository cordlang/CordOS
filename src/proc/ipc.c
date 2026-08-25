#include "ipc.h"
#include "phase9.h"
#include "serial.h"
#include "pci.h"
#include "virtio_net.h"
#include "wlan.h"
#include "nic.h"
#include "net.h"
#include "string.h"
#include "vga.h"

struct pipe {
    bool used;
    u8 buf[PIPE_BUF_SIZE];
    size_t head;
    size_t tail;
    size_t count;
};

static struct pipe pipes_os[PIPE_MAX];

i32 pipe_create(void)
{
    i32 i;

    for (i = 0; i < PIPE_MAX; ++i) {
        if (!pipes_os[i].used) {
            pipes_os[i].used = true;
            pipes_os[i].head = 0;
            pipes_os[i].tail = 0;
            pipes_os[i].count = 0;
            return i;
        }
    }

    return -1;
}

void pipe_close(i32 pipe_id)
{
    if (pipe_id < 0 || pipe_id >= PIPE_MAX) {
        return;
    }

    pipes_os[pipe_id].used = false;
    pipes_os[pipe_id].head = 0;
    pipes_os[pipe_id].tail = 0;
    pipes_os[pipe_id].count = 0;
}

ssize_t pipe_write(i32 pipe_id, const void *buf, size_t len)
{
    const u8 *src;
    size_t written = 0;

    if (pipe_id < 0 || pipe_id >= PIPE_MAX || !pipes_os[pipe_id].used || buf == NULL) {
        return -1;
    }

    src = (const u8 *)buf;
    while (written < len && pipes_os[pipe_id].count < PIPE_BUF_SIZE) {
        pipes_os[pipe_id].buf[pipes_os[pipe_id].tail] = src[written];
        pipes_os[pipe_id].tail = (pipes_os[pipe_id].tail + 1) % PIPE_BUF_SIZE;
        pipes_os[pipe_id].count++;
        written++;
    }

    return (ssize_t)written;
}

ssize_t pipe_read(i32 pipe_id, void *buf, size_t len)
{
    u8 *dst;
    size_t read_n = 0;

    if (pipe_id < 0 || pipe_id >= PIPE_MAX || !pipes_os[pipe_id].used || buf == NULL) {
        return -1;
    }

    dst = (u8 *)buf;
    while (read_n < len && pipes_os[pipe_id].count > 0) {
        dst[read_n] = pipes_os[pipe_id].buf[pipes_os[pipe_id].head];
        pipes_os[pipe_id].head = (pipes_os[pipe_id].head + 1) % PIPE_BUF_SIZE;
        pipes_os[pipe_id].count--;
        read_n++;
    }

    return (ssize_t)read_n;
}

static void pipe_self_test(void)
{
    i32 id;
    const char msg[] = "NOS";
    char out[4];
    ssize_t n;

    id = pipe_create();
    if (id < 0) {
        serial_write("ipc: pipe_create FAIL\n");
        return;
    }

    n = pipe_write(id, msg, 3);
    if (n != 3) {
        serial_write("ipc: pipe_write FAIL\n");
        pipe_close(id);
        return;
    }

    memset(out, 0, sizeof(out));
    n = pipe_read(id, out, 3);
    pipe_close(id);

    if (n != 3 || out[0] != 'N' || out[1] != 'O' || out[2] != 'S') {
        serial_write("ipc: pipe_read FAIL\n");
        return;
    }

    serial_write("ipc: pipe self-test OK\n");
}

void phase9_init(void)
{
    u32 n;

    n = pci_init();
    virtio_net_init();
    wlan_init();
    nic_init();
    net_init();
    pipe_self_test();

    serial_write("devices: pci=");
    serial_print_u32(n);
    serial_write("\n");
}
