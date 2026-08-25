#include "mouse.h"
#include "io.h"
#include "isr.h"
#include "pic.h"
#include "serial.h"

#define KBD_DATA    0x60
#define KBD_STATUS  0x64
#define MOUSE_Q     32

static volatile i32 s_x;
static volatile i32 s_y;
static volatile u8 s_buttons;
static volatile u32 s_w;
static volatile u32 s_h;
static volatile u8 s_packet[3];
static volatile u8 s_index;
static volatile bool s_ready;
static volatile struct mouse_event s_q[MOUSE_Q];
static volatile u32 s_head;
static volatile u32 s_tail;

static void mouse_wait_write(void)
{
    u32 spins = 100000;

    while ((inb(KBD_STATUS) & 2u) != 0 && spins > 0) {
        --spins;
    }
}

static void mouse_wait_read(void)
{
    u32 spins = 100000;

    while ((inb(KBD_STATUS) & 1u) == 0 && spins > 0) {
        --spins;
    }
}

static void mouse_write(u8 value)
{
    mouse_wait_write();
    outb(KBD_STATUS, 0xD4);
    mouse_wait_write();
    outb(KBD_DATA, value);
}

static u8 mouse_read(void)
{
    mouse_wait_read();
    return inb(KBD_DATA);
}

static void mouse_push(u8 kind, u8 button)
{
    u32 next = (s_head + 1u) % MOUSE_Q;
    volatile struct mouse_event *ev;

    if (next == s_tail) {
        return;
    }

    /* Coalesce consecutive moves. */
    if (kind == MOUSE_EV_MOVE && s_head != s_tail) {
        u32 last = (s_head + MOUSE_Q - 1u) % MOUSE_Q;
        if (s_q[last].kind == MOUSE_EV_MOVE) {
            s_q[last].x = s_x;
            s_q[last].y = s_y;
            s_q[last].buttons = s_buttons;
            return;
        }
    }

    ev = &s_q[s_head];
    ev->kind = kind;
    ev->button = button;
    ev->buttons = s_buttons;
    ev->x = s_x;
    ev->y = s_y;
    s_head = next;
}

void mouse_handle_byte(u8 data)
{
    i32 dx;
    i32 dy;
    u8 prev;
    u8 now;
    u8 bit;

    if (!s_ready) {
        return;
    }

    if (s_index == 0 && (data & 0x08u) == 0) {
        return;
    }

    s_packet[s_index++] = data;
    if (s_index < 3u) {
        return;
    }
    s_index = 0;

    if ((s_packet[0] & 0xC0u) != 0) {
        return;
    }

    dx = (i32)s_packet[1];
    dy = (i32)s_packet[2];
    if (s_packet[0] & 0x10u) {
        dx -= 256;
    }
    if (s_packet[0] & 0x20u) {
        dy -= 256;
    }

    s_x += dx;
    s_y -= dy;
    if (s_x < 0) {
        s_x = 0;
    }
    if (s_y < 0) {
        s_y = 0;
    }
    if (s_w > 0 && s_x >= (i32)s_w) {
        s_x = (i32)s_w - 1;
    }
    if (s_h > 0 && s_y >= (i32)s_h) {
        s_y = (i32)s_h - 1;
    }

    prev = s_buttons;
    now = (u8)(s_packet[0] & 0x07u);
    s_buttons = now;

    if (dx != 0 || dy != 0) {
        mouse_push(MOUSE_EV_MOVE, 0);
    }

    for (bit = 1; bit <= 4; bit <<= 1) {
        if ((now & bit) && !(prev & bit)) {
            mouse_push(MOUSE_EV_DOWN, bit);
        } else if (!(now & bit) && (prev & bit)) {
            mouse_push(MOUSE_EV_UP, bit);
        }
    }
}

static void mouse_irq(struct interrupt_frame *frame)
{
    u8 status;

    (void)frame;
    status = inb(KBD_STATUS);
    if ((status & 1u) == 0) {
        return;
    }
    mouse_handle_byte(inb(KBD_DATA));
}

void mouse_init(void)
{
    u8 status;

    s_x = 200;
    s_y = 150;
    s_buttons = 0;
    s_w = 0;
    s_h = 0;
    s_index = 0;
    s_head = 0;
    s_tail = 0;
    s_ready = false;

    mouse_wait_write();
    outb(KBD_STATUS, 0xA8);

    mouse_wait_write();
    outb(KBD_STATUS, 0x20);
    mouse_wait_read();
    status = inb(KBD_DATA);
    status |= 0x02u;
    status &= (u8)~0x20u;
    mouse_wait_write();
    outb(KBD_STATUS, 0x60);
    mouse_wait_write();
    outb(KBD_DATA, status);

    mouse_write(0xF6);
    (void)mouse_read();
    mouse_write(0xF4);
    (void)mouse_read();

    s_ready = true;
    irq_install_handler(12, mouse_irq);
    pic_clear_mask(2);
    pic_clear_mask(12);
    serial_write("mouse: ps/2 irq12 ready\n");
}

bool mouse_ready(void)
{
    return s_ready;
}

i32 mouse_x(void)
{
    i32 x = s_x;

    if (x < 0) {
        return 0;
    }
    if (s_w > 0 && x >= (i32)s_w) {
        return (i32)s_w - 1;
    }
    return x;
}

i32 mouse_y(void)
{
    i32 y = s_y;

    if (y < 0) {
        return 0;
    }
    if (s_h > 0 && y >= (i32)s_h) {
        return (i32)s_h - 1;
    }
    return y;
}

u8 mouse_buttons(void)
{
    return s_buttons;
}

void mouse_set_bounds(u32 width, u32 height)
{
    s_w = width;
    s_h = height;
    if (s_x >= (i32)s_w && s_w > 0) {
        s_x = (i32)s_w - 1;
    }
    if (s_y >= (i32)s_h && s_h > 0) {
        s_y = (i32)s_h - 1;
    }
}

bool mouse_has_event(void)
{
    return s_head != s_tail;
}

struct mouse_event mouse_get_event(void)
{
    struct mouse_event ev;

    ev.kind = 0;
    ev.button = 0;
    ev.buttons = 0;
    ev.x = s_x;
    ev.y = s_y;

    if (!mouse_has_event()) {
        return ev;
    }

    ev.kind = s_q[s_tail].kind;
    ev.button = s_q[s_tail].button;
    ev.buttons = s_q[s_tail].buttons;
    ev.x = s_q[s_tail].x;
    ev.y = s_q[s_tail].y;
    s_tail = (s_tail + 1u) % MOUSE_Q;
    return ev;
}
