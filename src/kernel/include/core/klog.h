#ifndef CORE_KLOG_H
#define CORE_KLOG_H

#include <typedef.h>
#include <core/bootparam.h>
#include <lock/mutex.h>

typedef enum
{
    KLOG_LEVEL_EMERG  = 0,
    KLOG_LEVEL_ALERT  = 1,
    KLOG_LEVEL_CRIT   = 2,
    KLOG_LEVEL_ERR    = 3,
    KLOG_LEVEL_WARN   = 4,
    KLOG_LEVEL_NOTICE = 5,
    KLOG_LEVEL_INFO   = 6,
    KLOG_LEVEL_DEBUG  = 7,
    KLOG_LEVEL_MAX    = 7
} klog_level;

extern mutex klog_flush_mutex;

void klog_init(const boot_param* param) __hidden;
void klog_start_background_thread(void) __hidden;

void klog(uint32 level, const char* format, ...);
void klog_flush(void);

#endif
