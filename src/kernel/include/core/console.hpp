#ifndef __CONSOLE_HPP__
#define __CONSOLE_HPP__

#include <typedef.hpp>

namespace console
{
    const uint8 CONSOLE_WIDTH = 80;
    const uint8 CONSOLE_HEIGHT = 25;
    
    const uint32 VIDEO_MEMORY_ADDRESS = 0xB8000;
    
    const uint8 CLEAR_CONSOLE_COLOR = 0x0F;
    
    extern void init();
    
    extern void clear();
    extern void draw_buffer(const uint8* buf, uint16 width, uint16 height, uint16 x_offset, uint16 y_offset);
    
    extern void move_cursor(uint16 x, uint16 y);
    extern void hide_cursor();
}

#endif
