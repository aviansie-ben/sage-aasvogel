#ifndef CORE_CONSOLE_H
#define CORE_CONSOLE_H

#include <typedef.h>

#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 25

typedef enum
{
    CONSOLE_COLOR_BLACK       = 0x0,
    CONSOLE_COLOR_BLUE        = 0x1,
    CONSOLE_COLOR_GREEN       = 0x2,
    CONSOLE_COLOR_CYAN        = 0x3,
    CONSOLE_COLOR_RED         = 0x4,
    CONSOLE_COLOR_MAGENTA     = 0x5,
    CONSOLE_COLOR_BROWN       = 0x6,
    CONSOLE_COLOR_LIGHT_GRAY  = 0x7,
    CONSOLE_COLOR_GRAY        = 0x8,
    CONSOLE_COLOR_LIGHT_BLUE  = 0x9,
    CONSOLE_COLOR_LIGHT_GREEN = 0xA,
    CONSOLE_COLOR_LIGHT_CYAN  = 0xB,
    CONSOLE_COLOR_LIGHT_RED   = 0xC,
    CONSOLE_COLOR_PINK        = 0xD,
    CONSOLE_COLOR_YELLOW      = 0xE,
    CONSOLE_COLOR_WHITE       = 0xF
} console_color;

typedef struct
{
    char ch;
    console_color fore_color : 4;
    console_color back_color : 4;
} __attribute__((packed)) console_char;

extern void console_init(void) __hidden;

extern void console_clear(console_char fill_char);
extern void console_draw_buffer(const console_char* buffer, uint16 x_off, uint16 y_off, uint16 width, uint16 height);

extern void console_move_cursor(uint16 x, uint16 y);
extern void console_hide_cursor(void);

#endif
