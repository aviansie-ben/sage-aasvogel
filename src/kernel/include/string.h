#ifndef STRING_H
#define STRING_H

#include <typedef.h>

extern void strcpy(char* dest, const char* src);
extern void strcat(char* dest, const char* src);
extern size_t strlen(const char* s);

extern char* itoa(int val, char* s, unsigned int base);
extern char* itoa_l(long long val, char* s, unsigned int base);

extern void* memcpy(void* dest, const void* src, size_t size);
extern void* memset(void* ptr, int val, size_t size);

#endif
