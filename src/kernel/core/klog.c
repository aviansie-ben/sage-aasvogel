#include <core/klog.h>
#include <core/tty.h>

static tty_vc* log_console;
static uint32 log_console_level;

static tty_serial* log_serial;
static uint32 log_serial_level;

static const char* level_names[] = {
    "EMERG",
    "ALERT",
    "CRIT",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG"
};

static const console_color level_colors[] = {
    CONSOLE_COLOR_LIGHT_RED,
    CONSOLE_COLOR_LIGHT_RED,
    CONSOLE_COLOR_LIGHT_RED,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_LIGHT_CYAN,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_GREEN
};
    

void klog_init(const boot_param* param)
{
    // TODO: Read these from the boot command-line
    log_console = &tty_virtual_consoles[0];
    log_console_level = KLOG_LEVEL_DEBUG;
    
    log_serial = &tty_serial_consoles[0];
    log_serial_level = KLOG_LEVEL_DEBUG;
}

static void log_message(tty_base* tty, uint32 level, const char* format, va_list vararg)
{
    spinlock_acquire(&tty->lock);
    tprintf(tty, "%Cf[%Cf%s%Cf] ", CONSOLE_COLOR_LIGHT_GRAY, level_colors[level], level_names[level], CONSOLE_COLOR_LIGHT_GRAY);
    tvprintf(tty, format, vararg);
    spinlock_release(&tty->lock);
}

void klog(uint32 level, const char* format, ...)
{
    va_list vararg;
    
    if (level > KLOG_LEVEL_MAX) level = KLOG_LEVEL_MAX;
    
    if (log_console != NULL && level <= log_console_level)
    {
        va_start(vararg, format);
        log_message(&log_console->base, level, format, vararg);
        va_end(vararg);
    }
    
    if (log_serial != NULL && level <= log_serial_level)
    {
        va_start(vararg, format);
        log_message(&log_serial->base, level, format, vararg);
        va_end(vararg);
    }
}
