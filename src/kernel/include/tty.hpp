#ifndef __TTY_HPP__
#define __TTY_HPP__

#include <typedef.hpp>

namespace tty
{
    enum TTYColor
    {
        COLOR_BLACK         = 0x0,
        COLOR_BLUE          = 0x1,
        COLOR_GREEN         = 0x2,
        COLOR_CYAN          = 0x3,
        COLOR_RED           = 0x4,
        COLOR_MAGENTA       = 0x5,
        COLOR_BROWN         = 0x6,
        COLOR_LIGHT_GRAY    = 0x7,
        COLOR_GRAY          = 0x8,
        COLOR_LIGHT_BLUE    = 0x9,
        COLOR_LIGHT_GREEN   = 0xA,
        COLOR_LIGHT_CYAN    = 0xB,
        COLOR_LIGHT_RED     = 0xC,
        COLOR_LIGHT_MAGENTA = 0xD,
        COLOR_YELLOW        = 0xE,
        COLOR_WHITE         = 0xF
    };

    class TTY
    {
    public:
        virtual void setColor(TTYColor bg, TTYColor fg) = 0;
        
        virtual void flush() = 0;
        virtual void clear() = 0;
        
        virtual TTY& operator <<(char c) = 0;
        virtual TTY& operator <<(const char* msg) = 0;
    };

    class TTYVirtualConsole : public TTY
    {
    public:
        TTYVirtualConsole(uint16 width, uint16 height);
        TTYVirtualConsole(uint16 width, uint16 height, uint8* buffer);
        TTYVirtualConsole(const TTYVirtualConsole& clone_of);
        ~TTYVirtualConsole();
        
        uint16 getWidth() const { return width; }
        uint16 getHeight() const { return height; }
        
        uint8* getBuffer() { return buffer; }
        const uint8* getBuffer() const { return buffer; }
        
        virtual TTYColor getBackColor() const { return (TTYColor)(cur_color & 0xF); }
        virtual TTYColor getForeColor() const { return (TTYColor)(cur_color >> 4); }
        
        virtual void setColor(TTYColor bg, TTYColor fg);
        
        bool isActive() const { return active; }
        void activate() {
            active = true;
            flush(); }
        void deactivate() { active = false; }
        
        bool isCursorShown() { return !cursor_hidden; }
        void hideCursor() { cursor_hidden = true; }
        void showCursor() { cursor_hidden = false; }
        
        virtual void flush();
        virtual void clear();
        
        TTYVirtualConsole& operator =(const TTYVirtualConsole& rhs);
        
        virtual TTYVirtualConsole& operator <<(char c);
        virtual TTYVirtualConsole& operator <<(const char* msg);
    private:
        uint16 width, height;
        
        bool cursor_hidden;
        uint16 cursor_x, cursor_y;
        
        uint8 cur_color;
        
        bool active, buffer_static;
        uint8* buffer;
    };

    extern TTYVirtualConsole boot_console;
    extern TTYVirtualConsole* active_console;
}

#endif
