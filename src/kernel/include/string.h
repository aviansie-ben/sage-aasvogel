#ifndef __STRING_H__
#define __STRING_H__

#include <typedef.h>

extern void strcpy(char* dest, const char* src);
extern void strcat(char* dest, const char* src);

extern char* itoa(int val, char* s, unsigned int base);

#endif
