#ifndef PRINTF_H
#define PRINTF_H

#include <typedef.h>
#include <stdarg.h>

typedef int (*gprintf_write_char)(void* i, char c);

int gprintf(const char* format, size_t n, void* i, gprintf_write_char write, va_list vararg);

int snprintf(char* s, size_t n, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list vararg);

#endif
