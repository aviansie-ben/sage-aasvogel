#ifndef STRING_H
#define STRING_H

#include <typedef.h>

extern const char* errcode_to_str(int errcode);

extern void strcpy(char* dest, const char* src);
extern void strncpy(char* dest, const char* src, size_t n);
extern void strcat(char* dest, const char* src);
extern size_t strlen(const char* s) __pure;
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, size_t n);

extern char* itoa(int val, char* s, unsigned int base);
extern char* itoa_l(long long val, char* s, unsigned int base);

extern long long strtonum(const char* str, long long minval, long long maxval, bool* error);
extern unsigned long strtoul(const char* src, char** endptr, int base);

extern void* memmove(void* dest, const void* src, size_t size);
extern void* memcpy(void* dest, const void* src, size_t size);
extern void* memset(void* ptr, int val, size_t size);
extern int memcmp(const void* p1, const void* p2, size_t size);

extern void* memcpy_safe(void* dest, const void* src, size_t size);

#endif
