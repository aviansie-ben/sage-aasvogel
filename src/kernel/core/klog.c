#include <core/klog.h>
#include <io/tty.h>

#include <stdarg.h>
#include <core/crash.h>
#include <printf.h>

#include <core/sched.h>
#include <memory/pool.h>
#include <string.h>
#include <lock/semaphore.h>

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

static const char* default_color = "37";

static const char* level_colors[] = {
    "91",
    "91",
    "91",
    "31",
    "93",
    "96",
    "36",
    "32"
};

typedef struct klog_buf
{
    uint32 level;
    struct klog_buf* next;

    char msg[];
} klog_buf;

mutex klog_flush_mutex;
static sched_thread* flush_thread;

static semaphore buf_semaphore;
static spinlock buf_lock;
static klog_buf* buf_head;
static klog_buf* buf_tail;

void klog_init(const boot_param* param)
{
    log_console = &tty_virtual_consoles[0];
    log_console_level = (uint32) cmdline_get_int(param, "klog_console_level", KLOG_LEVEL_MIN, KLOG_LEVEL_MAX, KLOG_LEVEL_INFO);

    log_serial = &tty_serial_consoles[cmdline_get_int(param, "klog_serial_port", 0, TTY_NUM_SERIAL - 1, 0)];
    log_serial_level = (uint32) cmdline_get_int(param, "klog_serial_level", KLOG_LEVEL_MIN, KLOG_LEVEL_MAX, KLOG_LEVEL_DISABLE);

    mutex_init(&klog_flush_mutex);
    semaphore_init(&buf_semaphore, 0);
}

static void klog_background_thread(void* a)
{
    while (true)
    {
        semaphore_wait(&buf_semaphore);

        mutex_acquire(&klog_flush_mutex);
        if (log_console != NULL) mutex_acquire(&log_console->base.lock);
        if (log_serial != NULL) mutex_acquire(&log_serial->base.lock);

        klog_flush();

        if (log_serial != NULL) mutex_release(&log_serial->base.lock);
        if (log_console != NULL) mutex_release(&log_console->base.lock);
        mutex_release(&klog_flush_mutex);
    }
}

void klog_start_background_thread(void)
{
    if (sched_thread_create(sched_process_current(), klog_background_thread, NULL, &flush_thread) != E_SUCCESS)
        crash("Failed to initialize klog background thread!");

    klog(KLOG_LEVEL_DEBUG, "Kernel background logging thread started!\n");
}

static void log_message_tty(tty_base* tty, uint32 level, const char* msg)
{
    tprintf(tty, "\33[%sm[\33[%sm%s\33[%sm] %s", default_color, level_colors[level - 1], level_names[level - 1], default_color, msg);
}

static void log_message(uint32 level, const char* msg)
{
    if (log_console != NULL && level <= log_console_level)
    {
        log_message_tty(&log_console->base, level, msg);
    }

    if (log_serial != NULL && level <= log_serial_level)
    {
        log_message_tty(&log_serial->base, level, msg);
    }
}

void klog(uint32 level, const char* format, ...)
{
    va_list vararg;
    char* msg;

    int msg_len;
    char msg_small[64];

    if (level > KLOG_LEVEL_MAX) level = KLOG_LEVEL_MAX;
    if (level > log_console_level && level > log_serial_level) return;

    va_start(vararg, format);
    msg_len = vsnprintf(msg_small, sizeof(msg_small), format, vararg);
    va_end(vararg);

    if (msg_len < 0)
    {
        crash("Error in vsnprintf?");
    }
    else if ((size_t)msg_len >= sizeof(msg_small))
    {
        msg = alloca((size_t)(msg_len + 1) * sizeof(char));

        va_start(vararg, format);
        msg_len = vsnprintf(msg, (size_t)msg_len + 1, format, vararg);
        va_end(vararg);

        if (msg_len < 0)
        {
            crash("Error in vsnprintf?");
        }
    }
    else
    {
        msg = msg_small;
    }

    if (flush_thread == NULL)
    {
        log_message(level, msg);
    }
    else
    {
        klog_buf* buf = kmem_pool_generic_alloc(sizeof(klog_buf) + (size_t)(msg_len + 1) * sizeof(char), 0);

        buf->level = level;
        buf->next = NULL;
        strcpy(buf->msg, msg);

        if (buf == NULL)
        {
            log_message(level, msg);
        }
        else
        {
            spinlock_acquire(&buf_lock);

            if (buf_tail == NULL)
            {
                buf_tail = buf_head = buf;
            }
            else
            {
                buf_tail->next = buf;
                buf_tail = buf;
            }

            semaphore_signal(&buf_semaphore);
            spinlock_release(&buf_lock);
        }
    }
}

void klog_flush()
{
    klog_buf* prev_buf;
    klog_buf* buf;

    if (flush_thread == NULL) return;

    spinlock_acquire(&buf_lock);
    while (semaphore_try_wait(&buf_semaphore)) ;
    buf = buf_head;
    buf_head = buf_tail = NULL;
    spinlock_release(&buf_lock);

    while (buf != NULL)
    {
        log_message(buf->level, buf->msg);

        prev_buf = buf;
        buf = buf->next;

        kmem_pool_generic_free(prev_buf);
    }
}
