#include <io/tty.h>
#include <io/console.h>
#include <assert.h>
#include <string.h>

#include <printf.h>

tty_vc tty_virtual_consoles[TTY_NUM_VCS];
static console_char tty_vc_buffers[TTY_NUM_VCS][CONSOLE_WIDTH * CONSOLE_HEIGHT];
static tty_vc* active_vc;

tty_serial tty_serial_consoles[TTY_NUM_SERIAL];

static console_color ansi_color_lookup[8] = {
    CONSOLE_COLOR_BLACK, CONSOLE_COLOR_RED, CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BROWN,
    CONSOLE_COLOR_BLUE, CONSOLE_COLOR_MAGENTA, CONSOLE_COLOR_CYAN, CONSOLE_COLOR_LIGHT_GRAY
};

static void tty_vc_flush(tty_base* base)
{
    tty_vc* tty = (tty_vc*) base;

    if (tty->is_active)
    {
        console_draw_buffer(&tty->buffer[base->width * tty->buffer_line], 0, 0, base->width, (uint16)(base->height - tty->buffer_line));
        console_draw_buffer(&tty->buffer[0], 0, (uint16)(base->height - tty->buffer_line), base->width, tty->buffer_line);

        if (base->cursor_hidden) console_hide_cursor();
        else console_move_cursor(base->cursor_x, base->cursor_y);
    }
}

static void tty_vc_clear(tty_base* base)
{
    tty_vc* tty = (tty_vc*) base;
    uint32 i;

    for (i = 0; i < (uint32)(base->width * base->height); i++)
    {
        tty->buffer[i].ch = ' ';

        if (tty->ansi_inverted)
        {
            tty->buffer[i].fore_color = tty->back_color;
            tty->buffer[i].back_color = tty->fore_color;
        }
        else
        {
            tty->buffer[i].fore_color = tty->fore_color;
            tty->buffer[i].back_color = tty->back_color;
        }
    }

    tty->buffer_line = 0;
    base->cursor_x = 0;
    base->cursor_y = 0;

    tty_vc_flush(base);
}

static void tty_vc_set_mode(tty_vc* tty, int m)
{
    if (m == 7)
        tty->ansi_inverted = true;
    else if (m == 27)
        tty->ansi_inverted = false;
    else if (m == 8)
        tty->ansi_hidden = true;
    else if (m == 28)
        tty->ansi_hidden = false;
    else if (m >= 30 && m <= 37)
        tty->fore_color = ansi_color_lookup[m - 30];
    else if (m >= 90 && m <= 97)
        tty->fore_color = ansi_color_lookup[m - 90] | 0x8;
    else if (m >= 40 && m <= 47)
        tty->back_color = ansi_color_lookup[m - 40];
    else if (m >= 100 && m <= 107)
        tty->back_color = ansi_color_lookup[m - 100] | 0x8;
}

static void tty_vc_ansi_mode_cmd(tty_vc* tty)
{
    int i;
    int m = 0;

    for (i = 0; i < tty->ansi_cmd_pos; i++)
    {
        if (tty->ansi_cmd_buf[i] >= '0' && tty->ansi_cmd_buf[i] <= '9')
        {
            m = (m * 10) + (tty->ansi_cmd_buf[i] - '0');
        }
        else if (tty->ansi_cmd_buf[i] == ';')
        {
            tty_vc_set_mode(tty, m);
            m = 0;
        }
    }

    tty_vc_set_mode(tty, m);
}

static void tty_vc_write(tty_base* base, char ch)
{
    tty_vc* tty = (tty_vc*) base;
    uint32 pos = (uint32)((((tty->buffer_line + base->cursor_y) % base->height) * base->width) + base->cursor_x);
    uint32 i;

    assert(base->cursor_x < base->width);
    assert(base->cursor_y < base->height);

    if (ch == '\33')
    {
        tty->ansi_cmd_pos = -2;
        return;
    }
    else if (tty->ansi_cmd_pos == -2)
    {
        if (ch == '[')
        {
            tty->ansi_cmd_pos = 0;
            return;
        }
        else
        {
            tty->ansi_cmd_pos = -1;
        }
    }
    else if (tty->ansi_cmd_pos >= 0)
    {
        if (ch >= '0' && ch <= '?')
        {
            if (tty->ansi_cmd_pos == sizeof(tty->ansi_cmd_buf))
            {
                tty->ansi_cmd_pos = -1;
            }
            else
            {
                tty->ansi_cmd_buf[tty->ansi_cmd_pos++] = ch;
            }
        }
        else
        {
            if (ch == 'm')
                tty_vc_ansi_mode_cmd(tty);

            tty->ansi_cmd_pos = -1;
        }

        return;
    }

    if (ch == '\n')
    {
        base->cursor_x = 0;
        base->cursor_y++;
    }
    else if (ch >= 0x20)
    {
        tty->buffer[pos].ch = (tty->ansi_hidden) ? ' ' : ch;

        if (tty->ansi_inverted)
        {
            tty->buffer[pos].fore_color = tty->back_color;
            tty->buffer[pos].back_color = tty->fore_color;
        }
        else
        {
            tty->buffer[pos].fore_color = tty->fore_color;
            tty->buffer[pos].back_color = tty->back_color;
        }

        base->cursor_x++;
    }

    if (base->cursor_x == base->width)
    {
        base->cursor_x = 0;
        base->cursor_y++;
    }

    if (base->cursor_y == base->height)
    {
        base->cursor_y--;

        for (i = 0; i < base->width; i++)
        {
            pos = (uint32)(tty->buffer_line * base->width) + i;

            tty->buffer[pos].ch = ' ';

            if (tty->ansi_inverted)
            {
                tty->buffer[pos].fore_color = tty->back_color;
                tty->buffer[pos].back_color = tty->fore_color;
            }
            else
            {
                tty->buffer[pos].fore_color = tty->fore_color;
                tty->buffer[pos].back_color = tty->back_color;
            }
        }

        tty->buffer_line = (uint16)(tty->buffer_line + 1) % base->height;
    }
}

static char tty_vc_read(tty_base* base)
{
    crash("Not yet implemented!");
}

static void tty_serial_flush(tty_base* base)
{
    // Do nothing
}

static void tty_serial_clear(tty_base* base)
{
    // TODO: Implement this?
}

static void tty_serial_write(tty_base* base, char ch)
{
    tty_serial* tty = (tty_serial*) base;

    spinlock_acquire(&tty->port->lock);
    switch (ch)
    {
        case '\n':
            serial_send(tty->port, "\r\n", 2, &base->lock);
            break;
        case '\b':
            serial_send(tty->port, "\b \b", 3, &base->lock);
            break;
        default:
            serial_send(tty->port, &ch, 1, &base->lock);
            break;
    }
    spinlock_release(&tty->port->lock);
}

static char tty_serial_read(tty_base* base)
{
    tty_serial* tty = (tty_serial*) base;

    char c;

    spinlock_acquire(&tty->port->lock);
    if (serial_receive(tty->port, &c, 1, &base->lock) != E_SUCCESS)
    {
        spinlock_release(&tty->port->lock);
        return '\0';
    }
    spinlock_release(&tty->port->lock);

    switch (c)
    {
        case '\r':
            return '\n';
        case '\x7f':
            return '\b';
        default:
            return c;
    }
}

static void tty_init_vc(tty_vc* tty, console_char* buffer, uint16 width, uint16 height)
{
    mutex_init(&tty->base.lock);

    tty->base.width = width;
    tty->base.height = height;

    tty->base.flush = tty_vc_flush;
    tty->base.clear = tty_vc_clear;
    tty->base.write = tty_vc_write;
    tty->base.read = tty_vc_read;

    tty->fore_color = CONSOLE_COLOR_WHITE;
    tty->back_color = CONSOLE_COLOR_BLACK;

    tty->ansi_hidden = false;
    tty->ansi_inverted = false;

    tty->ansi_cmd_pos = -1;

    tty->base.supports_cursor = true;
    tty->base.cursor_x = 0;
    tty->base.cursor_y = 0;
    tty->base.cursor_hidden = true;

    tty->buffer = buffer;
    tty->buffer_line = 0;
    tty->is_active = false;
}

static void tty_init_serial(tty_serial* tty, uint32 port_number)
{
    mutex_init(&tty->base.lock);

    // TODO: Find values for these?
    tty->base.width = 0;
    tty->base.height = 0;

    tty->base.flush = tty_serial_flush;
    tty->base.clear = tty_serial_clear;
    tty->base.write = tty_serial_write;
    tty->base.read = tty_serial_read;

    tty->base.supports_cursor = false;

    tty->port = &serial_ports[port_number];
}

void tty_init(void)
{
    uint32 i;

    for (i = 0; i < TTY_NUM_VCS; i++)
    {
        tty_init_vc(&tty_virtual_consoles[i], tty_vc_buffers[i], CONSOLE_WIDTH, CONSOLE_HEIGHT);
    }

    tty_virtual_consoles[0].is_active = true;
    tty_virtual_consoles[0].base.flush(&tty_virtual_consoles[0].base);
    active_vc = &tty_virtual_consoles[0];

    serial_init();
    for (i = 0; i < TTY_NUM_SERIAL; i++)
    {
        tty_init_serial(&tty_serial_consoles[i], i);
    }
}

void tty_init_interrupts(void)
{
    serial_init_interrupts();
}

void tty_write(tty_base* tty, const char* msg)
{
    while (*msg != '\0')
    {
        tty->write(tty, *msg);
        msg++;
    }
}

void tty_switch_vc(tty_vc* tty)
{
    tty_vc* old = active_vc;

    if (tty == old) return;

    mutex_acquire(&tty->base.lock);
    mutex_acquire(&old->base.lock);

    old->is_active = false;
    tty->is_active = true;
    active_vc = tty;

    mutex_release(&old->base.lock);

    tty->base.flush(&tty->base);
    mutex_release(&tty->base.lock);
}

static int tprintf_write_char(void* tty, char c)
{
    ((tty_base*)tty)->write((tty_base*)tty, c);
    return E_SUCCESS;
}

void tprintf(tty_base* tty, const char* format, ...)
{
    va_list vararg;

    va_start(vararg, format);
    gprintf(format, 0xffffffffu, tty, tprintf_write_char, vararg);
    tty->flush(tty);
    va_end(vararg);
}

void tvprintf(tty_base* tty, const char* format, va_list vararg)
{
    gprintf(format, 0xffffffffu, tty, tprintf_write_char, vararg);
    tty->flush(tty);
}
