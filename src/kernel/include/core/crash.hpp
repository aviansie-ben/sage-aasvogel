#ifndef __CRASH_HPP__
#define __CRASH_HPP__

#include <typedef.hpp>

#define crash(msg) do_crash(msg, __SOURCE_FILE__, __PRETTY_FUNCTION__, __LINE__)

extern void do_crash(const char* message, const char* file, const char* function, uint32 line) __attribute__((noreturn));

#endif
