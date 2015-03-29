#ifndef CORE_CRASH_H
#define CORE_CRASH_H

#include <typedef.h>

#define crash(msg) do_crash(msg, __SOURCE_FILE__, __PRETTY_FUNCTION__, __LINE__)

extern void do_crash(const char* msg, const char* file, const char* func, uint32 line) __attribute__((noreturn));
extern void do_crash_unhandled_isr(regs32_t* r);

#endif
