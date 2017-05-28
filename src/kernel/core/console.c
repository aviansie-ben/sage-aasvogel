#include <core/console.h>
#include <assert.h>
#include <hwio.h>

#define VIDEO_MEMORY_ADDRESS 0xC00B8000
#define BDA_PORT_ADDRESS 0xC0000463

static uint16 console_io_port;
static volatile console_char* console_video_buffer = (volatile console_char*) VIDEO_MEMORY_ADDRESS;

static bool cursor_hidden;

void console_init(void)
{
    // The BIOS data area contains the I/O port at which the video hardware can
    // be reached.
    console_io_port = *((uint16*)BDA_PORT_ADDRESS);

    // Before using the console, we should put it in a known state. This is done
    // by clearing the console and hiding the cursor.
    console_clear((console_char){ ' ', CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK });
    console_hide_cursor();
}

void console_clear(console_char fill_char)
{
    uint32 i;

    for (i = 0; i < CONSOLE_WIDTH * CONSOLE_HEIGHT; i++)
    {
        console_video_buffer[i] = fill_char;
    }
}

void console_draw_buffer(const console_char* buffer, uint16 x_off, uint16 y_off, uint16 width, uint16 height)
{
    uint32 x, y;

    for (y = 0; y < height && (y + y_off) < CONSOLE_HEIGHT; y++)
    {
        for (x = 0; x < width && (x + x_off) < CONSOLE_WIDTH; x++)
        {
            console_video_buffer[((y + y_off) * CONSOLE_WIDTH) + (x + x_off)] = buffer[(y * width) + x];
        }
    }
}

void console_move_cursor(uint16 x, uint16 y)
{
    uint16 pos = (uint16)((y * CONSOLE_WIDTH) + x);
    assert(console_io_port != 0);

    // Output the low byte of the cursor's position to the VGA I/O port
    outb(console_io_port, 0x0F);
    outb((uint16)(console_io_port + 1), (uint8)(pos & 0xFF));

    // Output the high byte of the cursor's position to the VGA I/O port
    outb(console_io_port, 0x0E);
    outb((uint16)(console_io_port + 1), (uint8)((pos >> 8) & 0xFF));

    // Detect whether the cursor is off the screen
    cursor_hidden = (pos >= (CONSOLE_HEIGHT * CONSOLE_WIDTH));
}

void console_hide_cursor(void)
{
    // If the cursor is already hidden, nothing needs to be done. Writing to the
    // VGA registers is time-consuming and we can avoid that here.
    if (cursor_hidden) return;

    // To hide the cursor, we need to simply move it outside of the screen
    // boundaries.
    console_move_cursor(0, CONSOLE_HEIGHT);
}
