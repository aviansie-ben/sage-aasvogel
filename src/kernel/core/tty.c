#include <core/tty.h>
#include <core/console.h>
#include <hwio.h>
#include <assert.h>
#include <string.h>

#define BDA_SERIAL_PORTS 0xC0000400

tty_vc tty_virtual_consoles[TTY_NUM_VCS];
static console_char tty_vc_buffers[TTY_NUM_VCS][CONSOLE_WIDTH * CONSOLE_HEIGHT];
static tty_vc* active_vc;

tty_serial tty_serial_consoles[TTY_NUM_SERIAL];

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
        tty->buffer[i].fore_color = base->fore_color;
        tty->buffer[i].back_color = base->back_color;
    }
    
    tty->buffer_line = 0;
    base->cursor_x = 0;
    base->cursor_y = 0;
    
    tty_vc_flush(base);
}

static void tty_vc_write(tty_base* base, char ch)
{
    tty_vc* tty = (tty_vc*) base;
    uint32 pos = (uint32)((((tty->buffer_line + base->cursor_y) % base->height) * base->width) + base->cursor_x);
    uint32 i;
    
    assert(base->cursor_x < base->width);
    assert(base->cursor_y < base->height);
    
    if (ch == '\n')
    {
        base->cursor_x = 0;
        base->cursor_y++;
    }
    else if (ch >= 0x20)
    {
        tty->buffer[pos].ch = ch;
        tty->buffer[pos].fore_color = base->fore_color;
        tty->buffer[pos].back_color = base->back_color;
        
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
            tty->buffer[pos].fore_color = base->fore_color;
            tty->buffer[pos].back_color = base->back_color;
        }
        
        tty->buffer_line = (uint16)(tty->buffer_line + 1) % base->height;
    }
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
    
    if (tty->io_port != 0)
    {
        if (ch == '\n')
            tty_serial_write(base, '\r');
        
        while ((inb((uint16) (tty->io_port + 5)) & 0x20) == 0) ;
        outb(tty->io_port, (uint8)ch);
    }
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
    
    for (i = 0; i < TTY_NUM_SERIAL; i++)
    {
        tty_init_serial(&tty_serial_consoles[i], *(((uint16*) BDA_SERIAL_PORTS) + i));
    }
}

void tty_init_vc(tty_vc* tty, console_char* buffer, uint16 width, uint16 height)
{
    spinlock_init(&tty->base.lock);
    
    tty->base.width = width;
    tty->base.height = height;
    
    tty->base.flush = tty_vc_flush;
    tty->base.clear = tty_vc_clear;
    tty->base.write = tty_vc_write;
    
    tty->base.supports_color = true;
    tty->base.fore_color = CONSOLE_COLOR_WHITE;
    tty->base.back_color = CONSOLE_COLOR_BLACK;
    
    tty->base.supports_cursor = true;
    tty->base.cursor_x = 0;
    tty->base.cursor_y = 0;
    tty->base.cursor_hidden = true;
    
    tty->buffer = buffer;
    tty->buffer_line = 0;
    tty->is_active = false;
}

void tty_init_serial(tty_serial* tty, uint16 io_port)
{
    spinlock_init(&tty->base.lock);
    
    // TODO: Find values for these?
    tty->base.width = 0;
    tty->base.height = 0;
    
    tty->base.flush = tty_serial_flush;
    tty->base.clear = tty_serial_clear;
    tty->base.write = tty_serial_write;
    
    tty->base.supports_color = false;
    tty->base.supports_cursor = false;
    
    tty->io_port = io_port;
    
    if (io_port != 0)
    {
        // Disable interrupts during configuration
        outb((uint16)(io_port + 1), 0x00);
        
        // TODO: Make the divisor and mode configurable in some way
        // Set the divisor to 12 (9600 baud)
        outb((uint16)(io_port + 3), 0x80);
        outb((uint16)(io_port + 0), 0x0C);
        outb((uint16)(io_port + 1), 0x00);
        
        // Set the mode to 8N1
        outb((uint16)(io_port + 3), 0x03);
        
        // Enable FIFO
        outb((uint16)(io_port + 2), 0xC7);
        
        // Enable interrupts
        outb((uint16)(io_port + 1), 0x0B);
    }
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
    
    if (tty == active_vc) return;
    
    spinlock_acquire(&tty->base.lock);
    spinlock_acquire(&old->base.lock);
    
    old->is_active = false;
    tty->is_active = true;
    active_vc = tty;
    
    spinlock_release(&old->base.lock);
    
    tty->base.flush(&tty->base);
    spinlock_release(&tty->base.lock);
}

static void tty_write_field(tty_base* tty, const char* msg, uint32 min_length, bool zero_pad, bool left_justify)
{
    uint32 length = strlen(msg);
    
    while (!left_justify && length < min_length)
    {
        length++;
        tty->write(tty, (zero_pad) ? '0' : ' ');
    }
    
    tty_write(tty, msg);
    
    while (left_justify && length < min_length)
    {
        length++;
        tty->write(tty, (zero_pad) ? '0' : ' ');
    }
}

static void print_formatted(tty_base* tty, const char** format, char* buf, va_list* vararg)
{
    const char* s;
    int i;
    long long l;
    
    uint32 min_length = 0;
    bool zero_pad = false;
    bool left_justify = false;
    
    if ((*format)[0] == '-')
    {
        *format += 1;
        left_justify = true;
    }
    
    if ((*format)[0] == '0')
    {
        *format += 1;
        zero_pad = true;
    }
    
    while ((*format)[0] >= '0' && (*format)[0] <= '9')
    {
        min_length = (min_length * 10) + (uint32)((*format)[0] - '0');
        *format += 1;
    }
    
    if ((*format)[0] == 's')
    {
        *format += 1;
        
        s = va_arg(*vararg, const char*);
        tty_write_field(tty, s, min_length, zero_pad, left_justify);
    }
    else if ((*format)[0] == 'd')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 10);
        tty_write_field(tty, buf, min_length, zero_pad, left_justify);
    }
    else if ((*format)[0] == 'x')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 16);
        tty_write_field(tty, buf, min_length, zero_pad, left_justify);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'd')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 10);
        tty_write_field(tty, buf, min_length, zero_pad, left_justify);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'x')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 16);
        tty_write_field(tty, buf, min_length, zero_pad, left_justify);
    }
    else if ((*format)[0] == 'C' && (*format)[1] == 'f')
    {
        *format += 2;
        
        i = va_arg(*vararg, console_color);
        
        if (tty->supports_color)
            tty->fore_color = i;
    }
    else if ((*format)[0] == 'C' && (*format)[1] == 'b')
    {
        *format += 2;
        
        i = va_arg(*vararg, console_color);
        
        if (tty->supports_color)
            tty->back_color = i;
    }
    else if ((*format)[0] == '%')
    {
        *format += 1;
        
        tty->write(tty, '%');
    }
    else
    {
        crash("Bad tprintf format string!");
    }
}

void tprintf(tty_base* tty, const char* format, ...)
{
    va_list vararg;
    
    va_start(vararg, format);
    tvprintf(tty, format, vararg);
    va_end(vararg);
}

void tvprintf(tty_base* tty, const char* format, va_list vararg)
{
    char ch;
    char buf[256];
    
    while ((ch = *(format++)) != '\0')
    {
        if (ch == '%')
        {
            print_formatted(tty, &format, buf, &vararg);
        }
        else
        {
            tty->write(tty, ch);
        }
    }
    
    tty->flush(tty);
}
