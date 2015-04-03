#include <core/tty.h>
#include <core/console.h>
#include <assert.h>
#include <string.h>

#include <stdarg.h>

tty_vc tty_virtual_consoles[TTY_NUM_VCS];
static console_char tty_vc_buffers[TTY_NUM_VCS][CONSOLE_WIDTH * CONSOLE_HEIGHT];
static tty_vc* active_vc;

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
        
        tty->buffer_line++;
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

static void print_formatted(tty_base* tty, const char** format, char* buf, va_list* vararg)
{
    const char* s;
    int i;
    long long l;
    
    if ((*format)[0] == 's')
    {
        *format += 1;
        
        s = va_arg(*vararg, const char*);
        tty_write(tty, s);
    }
    else if ((*format)[0] == 'd')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 10);
        tty_write(tty, buf);
    }
    else if ((*format)[0] == 'x')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 16);
        tty_write(tty, buf);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'd')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 10);
        tty_write(tty, buf);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'x')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 16);
        tty_write(tty, buf);
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
    
    char ch;
    char buf[256];
    
    va_start(vararg, format);
    
    spinlock_acquire(&tty->lock);
    
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
    
    spinlock_release(&tty->lock);
    va_end(vararg);
}
