#ifndef STRING_H
#define STRING_H

#include <typedef.h>

extern void strcpy(char* dest, const char* src);
extern void strncpy(char* dest, const char* src, size_t n);
extern void strcat(char* dest, const char* src);
extern size_t strlen(const char* s) __pure;
extern int strcmp(const char* s1, const char* s2);

extern char* itoa(int val, char* s, unsigned int base);
extern char* itoa_l(long long val, char* s, unsigned int base);

long long strtonum(const char* str, long long minval, long long maxval, bool* error);

extern void* memmove(void* dest, const void* src, size_t size);
extern void* memcpy(void* dest, const void* src, size_t size);
extern void* memset(void* ptr, int val, size_t size);

#endif
