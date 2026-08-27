#include "keyboard.h"
#include "io.h"
#include "isr.h"
#include "keycodes.h"
#ifdef __x86_64__
#include "mouse.h"
#endif

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_BUFFER_SIZE 128

static volatile u32 kbd_buffer[KBD_BUFFER_SIZE];
static volatile u32 kbd_head;
static volatile u32 kbd_tail;
static bool shift_pressed;
static bool altgr_pressed;
static bool e0_prefix;
static bool gui_pressed;
static bool lctrl_pressed;

/*
 * Scan code Set 1 — layout secuencial (probado). Indices = scancode.
 * 0x0E = Backspace, 0x1C = Enter.
 */
static const char scancode_set1[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' '
};

static const char scancode_set1_shift[] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' '
};

static void kbd_push(u32 codepoint)
{
    u32 next = (kbd_head + 1) % KBD_BUFFER_SIZE;

    if (next == kbd_tail) {
        /* Full: drop oldest so recent input is not silently lost. */
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    }

    kbd_buffer[kbd_head] = codepoint;
    kbd_head = next;
}

static u32 keyboard_altgr_codepoint(char character)
{
    switch (character) {
    case 'a': return 0x00E1u;
    case 'A': return 0x00C1u;
    case 'e': return 0x00E9u;
    case 'E': return 0x00C9u;
    case 'i': return 0x00EDu;
    case 'I': return 0x00CDu;
    case 'o': return 0x00F3u;
    case 'O': return 0x00D3u;
    case 'u': return 0x00FAu;
    case 'U': return 0x00DAu;
    case 'y': return 0x00FCu;
    case 'Y': return 0x00DCu;
    case 'n': return 0x00F1u;
    case 'N': return 0x00D1u;
    case 'c': return 0x00E7u;
    case 'C': return 0x00C7u;
    case '1': return 0x00A1u;
    case '!': return 0x00A1u;
    case '?': return 0x00BFu;
    default: return (u32)(u8)character;
    }
}

static void kbd_wait_write(void)
{
    u32 spins = 100000;

    while ((inb(KBD_STATUS_PORT) & 2u) != 0 && spins > 0) {
        --spins;
    }
}

static void kbd_drain_output(void)
{
    u32 spins = 100000;

    while (spins > 0 && (inb(KBD_STATUS_PORT) & 1u) != 0) {
        (void)inb(KBD_DATA_PORT);
        --spins;
    }
}

static void keyboard_irq(struct interrupt_frame *frame)
{
    u8 scancode;
    char character = 0;
    u32 codepoint;
    bool extended;

    (void)frame;
    {
        u8 status = inb(KBD_STATUS_PORT);
        if ((status & 1u) == 0) {
            return;
        }
#ifdef __x86_64__
        if (status & 0x20u) {
            mouse_handle_byte(inb(KBD_DATA_PORT));
            return;
        }
#endif
    }
    scancode = inb(KBD_DATA_PORT);

    if (scancode == 0xE0) {
        e0_prefix = true;
        return;
    }

    extended = e0_prefix;
    e0_prefix = false;

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }

    if (scancode == 0x38) {
        if (extended) {
            altgr_pressed = true;
        }
        return;
    }

    if (scancode == 0xB8) {
        if (extended) {
            altgr_pressed = false;
        }
        return;
    }

    /* Left/Right GUI (Windows / Super). Releases have bit 7 set. */
    if (extended && (scancode == 0x5B || scancode == 0x5C)) {
        gui_pressed = true;
        return;
    }
    if (extended && (scancode == 0xDB || scancode == 0xDC)) {
        gui_pressed = false;
        return;
    }

    /* Left Ctrl — fallback for hosts that swallow the Windows key (VBox). */
    if (!extended && scancode == 0x1D) {
        lctrl_pressed = true;
        return;
    }
    if (!extended && scancode == 0x9D) {
        lctrl_pressed = false;
        return;
    }

    if (scancode & 0x80) {
        return;
    }

    /* Flechas / Home (E0) y F1 para UI de sesión. */
    if (extended) {
        if (scancode == 0x48) {
            kbd_push(KEY_UP);
            return;
        }
        if (scancode == 0x50) {
            kbd_push(KEY_DOWN);
            return;
        }
        if (scancode == 0x4B) {
            kbd_push(KEY_LEFT);
            return;
        }
        if (scancode == 0x4D) {
            kbd_push(KEY_RIGHT);
            return;
        }
        if (scancode == 0x47) {
            kbd_push(KEY_HOME);
            return;
        }
    }

    if (scancode == 0x3B) {
        kbd_push(KEY_F1);
        return;
    }

    /* Enter principal / teclado numérico (E0 1C) */
    if (scancode == 0x1C) {
        character = '\n';
    } else if (scancode == 0x0E) {
        character = '\b';
    } else if (scancode == 0x53) {
        character = '\b'; /* Supr → borrar */
    } else if (scancode < sizeof(scancode_set1)) {
        character = shift_pressed
            ? scancode_set1_shift[scancode]
            : scancode_set1[scancode];
    }

    if (character != 0) {
        if (character == ' ' && (gui_pressed || lctrl_pressed)) {
            kbd_push(KEY_SPOTLIGHT);
            return;
        }
        codepoint = altgr_pressed
            ? keyboard_altgr_codepoint(character)
            : (u32)(u8)character;
        kbd_push(codepoint);
    }
}

void keyboard_init(void)
{
    kbd_head = 0;
    kbd_tail = 0;
    shift_pressed = false;
    altgr_pressed = false;
    e0_prefix = false;
    gui_pressed = false;
    lctrl_pressed = false;

    /* Do not let a stale ACK be mistaken for the 8042 command byte. */
    kbd_drain_output();
    kbd_wait_write();
    outb(KBD_DATA_PORT, 0xF4);
    kbd_drain_output();

    irq_install_handler(1, keyboard_irq);
}

bool keyboard_has_char(void)
{
    return kbd_head != kbd_tail;
}

u32 keyboard_get_codepoint(void)
{
    u32 codepoint;

    if (!keyboard_has_char()) {
        return 0;
    }

    codepoint = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return codepoint;
}

char keyboard_getchar(void)
{
    u32 codepoint = keyboard_get_codepoint();

    return codepoint <= 0x7Fu ? (char)codepoint : '?';
}
