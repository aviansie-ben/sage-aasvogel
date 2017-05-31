#ifndef CORE_TTY_H
#define CORE_TTY_H

#include <io/console.h>
#include <io/serial.h>
#include <lock/mutex.h>
#include <lock/condvar.h>
#include <stdarg.h>

#define TTY_NUM_VCS 5
#define TTY_NUM_SERIAL 4

typedef struct tty_base
{
    mutex lock;

    uint16 width, height;

    void (*flush)(struct tty_base*);
    void (*clear)(struct tty_base*);
    void (*write)(struct tty_base*, char);
    char (*read)(struct tty_base*);

    bool supports_cursor;
    uint16 cursor_x, cursor_y;
    bool cursor_hidden;
} tty_base;

typedef struct
{
    tty_base base;

    console_color fore_color;
    console_color back_color;

    char ansi_cmd_buf[32];
    int ansi_cmd_pos;

    bool ansi_hidden;
    bool ansi_inverted;

    console_char* buffer;
    uint16 buffer_line;

    bool is_active;
} tty_vc;

typedef struct
{
    tty_base base;
    serial_port* port;
} tty_serial;

extern tty_vc tty_virtual_consoles[TTY_NUM_VCS];
extern tty_serial tty_serial_consoles[TTY_NUM_SERIAL];

extern void tty_init(void) __hidden;
extern void tty_init_interrupts(void) __hidden;

extern void tty_write(tty_base* tty, const char* msg);

extern void tty_switch_vc(tty_vc* tty);

extern void tprintf(tty_base* tty, const char* format, ...);
extern void tvprintf(tty_base* tty, const char* format, va_list vararg);

#endif
