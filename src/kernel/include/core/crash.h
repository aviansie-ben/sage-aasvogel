#ifndef CORE_CRASH_H
#define CORE_CRASH_H

#include <typedef.h>

#define CRASH_STACKTRACE_MAX_DEPTH 15
#define crash(msg) do_crash(msg, __SOURCE_FILE__, __PRETTY_FUNCTION__, __LINE__)
#define crash_interrupt(msg, r) do_crash_interrupt(msg, __SOURCE_FILE__, __PRETTY_FUNCTION__, __LINE__, r)

extern void do_crash(const char* msg, const char* file, const char* func, uint32 line) __attribute__((noreturn));
extern void do_crash_interrupt(const char* msg, const char* file, const char* func, uint32 line, regs32_t* r) __attribute__((noreturn));
extern void do_crash_pagefault(regs32_t* r) __attribute__((noreturn));
extern void do_crash_unhandled_isr(regs32_t* r) __attribute__((noreturn));

#endif
