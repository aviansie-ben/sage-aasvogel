#include <core/console.hpp>
#include <tty.hpp>

namespace tty
{
    const uint8 DEFAULT_VIRTUAL_CONSOLE_COLOR = 0x0F;

    static uint8 boot_console_buf[console::CONSOLE_WIDTH * console::CONSOLE_HEIGHT * 2];
    TTYVirtualConsole boot_console(console::CONSOLE_WIDTH, console::CONSOLE_HEIGHT, boot_console_buf);

    TTYVirtualConsole* active_console = &boot_console;
    
    TTY& TTY::operator <<(const char* msg)
    {
        while (*msg != '\0')
        {
            *this << *msg;
            msg++;
        }
        
        return *this;
    }
    
    TTY& TTY::operator <<(uint64 num)
    {
        const char* chars = "0123456789ABCDEF";
        char temp[65];
        uint32 i = 0;
        
        // Use successive division to find each character
        while (num != 0)
        {
            temp[i++] = chars[num % num_base];
            num /= num_base;
        }
        
        // Because successive division starts with the least significant digit, we must
        // reverse the string to display it properly.
        for (uint32 j = 0; j < (i / 2); j++)
        {
            char t = temp[j];
            temp[j] = temp[i - j - 1];
            temp[i - j - 1] = t;
        }
        
        // If we had a zero, successive division would return an empty string. We don't
        // want that, so if we get an empty string, append a 0.
        if (i == 0)
            temp[i++] = chars[0];
        
        // Add a null terminator to the end of the string
        temp[i] = '\0';
        
        // Actually print out the string...
        return *this << temp;
    }
    
    TTY& TTY::operator <<(uint32 num)
    {
        // There is a second function for 32-bit integers because 64-bit arithmetic
        // makes function calls to libgcc, whereas 32-bit arithmetic does not. We
        // try to avoid making those function calls unless absolutely necessary.
        
        const char* chars = "0123456789ABCDEF";
        char temp[33];
        uint32 i = 0;
        
        // Use successive division to find each character
        while (num != 0)
        {
            temp[i++] = chars[num % num_base];
            num /= num_base;
        }
        
        // Because successive division starts with the least significant digit, we must
        // reverse the string to display it properly.
        for (uint32 j = 0; j < (i / 2); j++)
        {
            char t = temp[j];
            temp[j] = temp[i - j - 1];
            temp[i - j - 1] = t;
        }
        
        // If we had a zero, successive division would return an empty string. We don't
        // want that, so if we get an empty string, append a 0.
        if (i == 0)
            temp[i++] = chars[0];
        
        // Add a null terminator to the end of the string
        temp[i] = '\0';
        
        // Actually print out the string...
        return *this << temp;
    }
    
    TTY& TTY::operator <<(int64 num)
    {
        // If the number is negative, print out the negative sign...
        if (num < 0)
        {
            *this << '-';
            num = -num;
        }
        
        // We defer to the unsigned version to actually display the number.
        return *this << ((uint64)num);
    }
    
    TTY& TTY::operator <<(int32 num)
    {
        // If the number is negative, print out the negative sign...
        if (num < 0)
        {
            *this << '-';
            num = -num;
        }
        
        // We defer to the unsigned version to actually display the number.
        return *this << ((uint32)num);
    }

    TTYVirtualConsole::TTYVirtualConsole(uint16 width, uint16 height, uint8* buffer)
        : width(width), height(height), cursor_hidden(false), cursor_x(0), cursor_y(0),
          cur_color(DEFAULT_VIRTUAL_CONSOLE_COLOR), active(false), buffer_static(true),
          buffer_line(0), buffer(buffer)
    {
        clear();
    }

    TTYVirtualConsole::TTYVirtualConsole(uint16 width, uint16 height)
        : width(width), height(height), cursor_hidden(false), cursor_x(0), cursor_y(0),
          cur_color(DEFAULT_VIRTUAL_CONSOLE_COLOR), active(false), buffer_static(false),
          buffer_line(0), buffer(0)
    {
        buffer = new uint8[width * height * 2];
        clear();
    }

    TTYVirtualConsole::TTYVirtualConsole(const TTYVirtualConsole& clone_of)
        : width(clone_of.width), height(clone_of.height), cursor_hidden(clone_of.cursor_hidden),
          cursor_x(clone_of.cursor_x), cursor_y(clone_of.cursor_y), cur_color(clone_of.cur_color),
          active(false), buffer_static(false), buffer_line(clone_of.buffer_line), buffer(0)
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
            console::draw_buffer(buffer + (width * buffer_line * 2), width, height - buffer_line, 0, 0);
            if (buffer_line != 0) console::draw_buffer(buffer, width, buffer_line, 0, height - buffer_line);
            
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
        
        buffer_line = rhs.buffer_line;
        for (uint32 i = 0; i < (uint32)(width * height); i++)
            buffer[i] = rhs.buffer[i];
        
        flush();
        
        return *this;
    }

    TTY& TTYVirtualConsole::operator <<(char c)
    {
        if (c == '\n')
        {
            cursor_x = 0;
            cursor_y++;
            
            if (cursor_y >= height) rotate_buffer();
            
            flush();
        }
        else
        {
            uint32 buf_pos = ((((cursor_y + buffer_line) % height) * width) + cursor_x) * 2;
            
            buffer[buf_pos] = c;
            buffer[buf_pos + 1] = cur_color;
            
            cursor_x++;
            if (cursor_x >= width) { cursor_y++; cursor_x = 0; }
            
            if (cursor_y >= height) rotate_buffer();
        }
        
        return *this;
    }
    
    void TTYVirtualConsole::rotate_buffer()
    {
        for (uint16 x = 0; x < width; x++)
        {
            uint32 buf_pos = ((buffer_line * width) + x) * 2;
            
            buffer[buf_pos] = ' ';
            buffer[buf_pos + 1] = cur_color;
        }
        
        cursor_y--;
        buffer_line++;
        
        if (buffer_line >= height) buffer_line = 0;
    }
}
