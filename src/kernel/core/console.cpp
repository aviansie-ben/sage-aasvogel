#include <core/console.hpp>
#include <hwio.hpp>

namespace console
{
    static volatile uint8* video_buffer = (volatile uint8*) (VIDEO_MEMORY_ADDRESS + 0xC0000000);
    static uint16 io_port = 0;
    
    static bool cursor_hidden = false;
    
    void init()
    {
        // The BIOS data area contains the I/O port at which the video hardware can be reached
        io_port = *((uint16*) (0x463 + 0xC0000000));
        
        // Before we use the screen, we should put it into a known state by clearing it and moving
        // the cursor to position (0, 0)
        clear();
        move_cursor(0, 0);
    }
    
    void clear()
    {
        for (uint32 i = 0; i < (CONSOLE_HEIGHT * CONSOLE_WIDTH); i++)
        {
            video_buffer[i * 2] = ' ';
            video_buffer[(i * 2) + 1] = CLEAR_CONSOLE_COLOR;
        }
    }
    
    void draw_buffer(const uint8* buf, uint16 width, uint16 height, uint16 x_offset, uint16 y_offset)
    {
        for (int x = 0; x < width && x < (CONSOLE_WIDTH - x_offset); x++)
        {
            for (int y = 0; y < height && y < (CONSOLE_HEIGHT - y_offset); y++)
            {
                uint16 pos_buf = ((y * width) + x) * 2;
                uint16 pos_mem = (((y + y_offset) * CONSOLE_WIDTH) + (x + x_offset)) * 2;
                
                video_buffer[pos_mem] = buf[pos_buf];
                video_buffer[pos_mem + 1] = buf[pos_buf + 1];
            }
        }
    }
    
    void move_cursor(uint16 x, uint16 y)
    {
        uint16 pos = (y * CONSOLE_WIDTH) + x;
        
        // Output the low byte of the cursor to the VGA I/O port
        outb(io_port, 0x0F);
        outb(io_port + 1, (uint8)(pos & 0xFF));
        
        // Output the high byte of the cursor to the VGA I/O port
        outb(io_port, 0x0E);
        outb(io_port + 1, (uint8)((pos >> 8) & 0xFF));
        
        // Detect whether the cursor is outside of the screen bounds
        cursor_hidden = (pos >= (CONSOLE_HEIGHT * CONSOLE_WIDTH));
    }
    
    void hide_cursor()
    {
        // If the cursor is already hidden, do nothing. Writing to the VGA I/O port is
        // quite time-consuming, so we should avoid doing so when possible.
        if (cursor_hidden) return;
        
        // To hide the cursor, we move it beyond the bounds of the screen
        move_cursor(0, CONSOLE_HEIGHT);
    }
}
