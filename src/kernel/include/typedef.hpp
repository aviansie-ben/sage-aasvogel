#ifndef __TYPEDEF_HPP__
#define __TYPEDEF_HPP__

#include <stddef.h>

// Some features (e.g. assert) require that __PRETTY_FUNCTION__ be defined. If we
// aren't using g++, we should fall back to __func__.
#ifndef __GNUC__
	#define __PRETTY_FUNCTION__ __func__
#endif

#define magic_breakpoint() asm volatile ("xchg %bx, %bx");
#define hang() magic_breakpoint(); while (true) { asm volatile ("cli; hlt"); }

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef int8_t int8;
typedef uint8_t uint8;

typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef int16_t int16;
typedef uint16_t uint16;

typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef int32_t int32;
typedef uint32_t uint32;

typedef signed long long int64_t;
typedef unsigned long long uint64_t;
typedef int64_t int64;
typedef uint64_t uint64;

typedef struct
{
    uint32 gs, fs, es, ds;
    uint32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32 int_no, err_code;
    uint32 eip, cs, eflags;
    
    uint32 useresp, ss;
} regs32_t;
typedef regs32_t regs32;

#endif
