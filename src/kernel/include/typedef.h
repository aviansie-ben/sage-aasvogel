#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <stddef.h>
#include <stdbool.h>

// Some features (e.g. assert) require that __PRETTY_FUNCTION__ be defined. If we
// aren't using gcc, we should fall back to __func__.
#ifndef __GNUC__
	#define __PRETTY_FUNCTION__ __func__
#endif

// The build script usually defines __SOURCE_FILE__ for us when building. If, for
// some reason, it's missing, fall back to __FILE__.
#ifndef __SOURCE_FILE__
    #define __SOURCE_FILE__ __FILE__
#endif

#define magic_breakpoint() asm volatile ("xchg %bx, %bx");
#define hang() magic_breakpoint(); while (true) { asm volatile ("cli; hlt"); }
#define hang_soft() magic_breakpoint(); while (true) { asm volatile ("hlt"); }

#define __hidden __attribute__((visibility("hidden")))
#define __pure __attribute__((pure))
#define __const __attribute__((const))
#define __warn_unused_result __attribute__((warn_unused_result))

#define alloca(size) __builtin_alloca(size)

#define REALLY_PASTE(x,y) x##y
#define PASTE(x,y) REALLY_PASTE(x,y)

#define E_SUCCESS         0
#define E_NOT_FOUND       1
#define E_NOT_SUPPORTED   2
#define E_IO_ERROR        3
#define E_INVALID         4
#define E_BUSY            5
#define E_NOT_DIR         6
#define E_IS_DIR          7
#define E_NAME_TOO_LONG   8
#define E_LOOP            9
#define E_NO_SPACE       10
#define E_NO_MEMORY      11
#define E_ALREADY_EXISTS 12

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
    uint32 edi, esi, ebp, unused_esp, ebx, edx, ecx, eax;
    uint32 int_no, err_code;
    uint32 eip, cs, eflags;
    
    uint32 esp, ss;
} regs32_t;
typedef regs32_t regs32;

typedef struct
{
    uint32 gs, fs, es, ds;
    uint32 edi, esi, ebp, ebx, edx, ecx, eax;
    uint32 eip, cs, eflags;
    
    uint32 esp, ss;
} regs32_saved_t;

#endif
