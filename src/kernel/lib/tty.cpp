#include <core/console.hpp>
#include <tty.hpp>

namespace tty
{
    const uint8 DEFAULT_VIRTUAL_CONSOLE_COLOR = 0x0F;

    static uint8 boot_console_buf[console::CONSOLE_WIDTH * console::CONSOLE_HEIGHT * 2];
    TTYVirtualConsole boot_console(console::CONSOLE_WIDTH, console::CONSOLE_HEIGHT, boot_console_buf);

    TTYVirtualConsole* active_console = &boot_console;

    TTYVirtualConsole::TTYVirtualConsole(uint16 width, uint16 height, uint8* buffer)
        : width(width), height(height), cursor_hidden(false), cursor_x(0), cursor_y(0),
          cur_color(DEFAULT_VIRTUAL_CONSOLE_COLOR), active(false), buffer_static(true), buffer(buffer)
    {
        clear();
    }

    TTYVirtualConsole::TTYVirtualConsole(uint16 width, uint16 height)
        : width(width), height(height), cursor_hidden(false), cursor_x(0), cursor_y(0),
          cur_color(DEFAULT_VIRTUAL_CONSOLE_COLOR), active(false), buffer_static(false), buffer(0)
    {
        buffer = new uint8[width * height * 2];
        clear();
    }

    TTYVirtualConsole::TTYVirtualConsole(const TTYVirtualConsole& clone_of)
        : width(clone_of.width), height(clone_of.height), cursor_hidden(clone_of.cursor_hidden),
          cursor_x(clone_of.cursor_x), cursor_y(clone_of.cursor_y), cur_color(clone_of.cur_color),
          active(false), buffer_static(false), buffer(0)
    {
        buffer = new uint8[width * height * 2];
        
        for (uint32 i = 0; i < (uint32)(width * height); i++)
            buffer[i] = clone_of.buffer[i];
    }

    TTYVirtualConsole::~TTYVirtualConsole()
    {
        if (!buffer_static) delete [] buffer;
    }

    void TTYVirtualConsole::setColor(TTYColor bg, TTYColor fg)
    {
        cur_color = (bg << 4) | fg;
    }

    void TTYVirtualConsole::flush()
    {
        if (active)
        {
            console::draw_buffer(buffer, width, height, 0, 0);
            
            if (cursor_hidden) console::hide_cursor();
            else console::move_cursor(cursor_x, cursor_y);
        }
    }

    void TTYVirtualConsole::clear()
    {
        for (uint32 i = 0; i < (uint32)(width * height); i++)
        {
            buffer[i * 2] = ' ';
            buffer[(i * 2) + 1] = cur_color;
        }
        
        flush();
    }

    TTYVirtualConsole& TTYVirtualConsole::operator =(const TTYVirtualConsole& rhs)
    {
        if (&rhs == this) return *this;
        
        if (rhs.width != width || rhs.height != height)
        {
            if (!buffer_static) delete [] buffer;
        
            width = rhs.width;
            height = rhs.height;
            
            buffer_static = false;
            buffer = new uint8[width * height * 2];
        }
        
        cursor_hidden = rhs.cursor_hidden;
        cursor_x = rhs.cursor_x;
        cursor_y = rhs.cursor_y;
        cur_color = rhs.cur_color;
        
        for (uint32 i = 0; i < (uint32)(width * height); i++)
            buffer[i] = rhs.buffer[i];
        
        flush();
        
        return *this;
    }

    TTYVirtualConsole& TTYVirtualConsole::operator <<(char c)
    {
        if (c == '\n')
        {
            cursor_x = 0;
            cursor_y++;
            
            flush();
        }
        else
        {
            uint32 buf_pos = ((cursor_y * width) + cursor_x) * 2;
            
            buffer[buf_pos] = c;
            buffer[buf_pos + 1] = cur_color;
            
            cursor_x++;
            if (cursor_x >= width) { cursor_y++; cursor_x = 0; }
        }
        
        return *this;
    }

    TTYVirtualConsole& TTYVirtualConsole::operator <<(const char* msg)
    {
        while (*msg != '\0')
        {
            *this << *msg;
            msg++;
        }
        
        return *this;
    }
}
