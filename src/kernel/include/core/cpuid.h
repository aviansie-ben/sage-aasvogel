#ifndef CORE_CPUID_H
#define CORE_CPUID_H

#include <typedef.h>

typedef union
{
    char str[13];
    uint32 regs[3];
} cpuid_vendor_id;

typedef struct
{
    cpuid_vendor_id vendor_id;
    const char* vendor_name;
} cpuid_vendor;

typedef enum
{
    CPUID_FEATURE_EDX_FPU   = (1 << 0),
    CPUID_FEATURE_EDX_VME   = (1 << 1),
    CPUID_FEATURE_EDX_DE    = (1 << 2),
    CPUID_FEATURE_EDX_PSE   = (1 << 3),
    CPUID_FEATURE_EDX_TSC   = (1 << 4),
    CPUID_FEATURE_EDX_MSR   = (1 << 5),
    CPUID_FEATURE_EDX_PAE   = (1 << 6),
    CPUID_FEATURE_EDX_MCE   = (1 << 7),
    CPUID_FEATURE_EDX_CX8   = (1 << 8),
    CPUID_FEATURE_EDX_APIC  = (1 << 9),
    CPUID_FEATURE_EDX_SEP   = (1 << 11),
    CPUID_FEATURE_EDX_MTRR  = (1 << 12),
    CPUID_FEATURE_EDX_PGE   = (1 << 13),
    CPUID_FEATURE_EDX_MCA   = (1 << 14),
    CPUID_FEATURE_EDX_CMOV  = (1 << 15),
    CPUID_FEATURE_EDX_PAT   = (1 << 16),
    CPUID_FEATURE_EDX_PSE36 = (1 << 17),
    CPUID_FEATURE_EDX_PSN   = (1 << 18),
    CPUID_FEATURE_EDX_CLF   = (1 << 19),
    CPUID_FEATURE_EDX_DS    = (1 << 21),
    CPUID_FEATURE_EDX_ACPI  = (1 << 22),
    CPUID_FEATURE_EDX_MMX   = (1 << 23),
    CPUID_FEATURE_EDX_FXSR  = (1 << 24),
    CPUID_FEATURE_EDX_SSE   = (1 << 25),
    CPUID_FEATURE_EDX_SSE2  = (1 << 26),
    CPUID_FEATURE_EDX_SS    = (1 << 27),
    CPUID_FEATURE_EDX_HTT   = (1 << 28),
    CPUID_FEATURE_EDX_TM    = (1 << 29),
    CPUID_FEATURE_EDX_IA64  = (1 << 30),
    CPUID_FEATURE_EDX_PBE   = (1 << 31)
} cpuid_feature_edx;

typedef enum
{
    CPUID_FEATURE_ECX_SSE3      = (1 << 0),
    CPUID_FEATURE_ECX_PCLMULQDQ = (1 << 1),
    CPUID_FEATURE_ECX_DTES64    = (1 << 2),
    CPUID_FEATURE_ECX_MONITOR   = (1 << 3),
    CPUID_FEATURE_ECX_DSCPL     = (1 << 4),
    CPUID_FEATURE_ECX_VMX       = (1 << 5),
    CPUID_FEATURE_ECX_SMX       = (1 << 6),
    CPUID_FEATURE_ECX_EST       = (1 << 7),
    CPUID_FEATURE_ECX_TM2       = (1 << 8),
    CPUID_FEATURE_ECX_SSSE3     = (1 << 9),
    CPUID_FEATURE_ECX_CNXTID    = (1 << 10),
    CPUID_FEATURE_ECX_SDBG      = (1 << 11),
    CPUID_FEATURE_ECX_FMA       = (1 << 12),
    CPUID_FEATURE_ECX_CX16      = (1 << 13),
    CPUID_FEATURE_ECX_XTPR      = (1 << 14),
    CPUID_FEATURE_ECX_PDCM      = (1 << 15),
    CPUID_FEATURE_ECX_PCID      = (1 << 17),
    CPUID_FEATURE_ECX_DCA       = (1 << 18),
    CPUID_FEATURE_ECX_SSE41     = (1 << 19),
    CPUID_FEATURE_ECX_SSE42     = (1 << 20),
    CPUID_FEATURE_ECX_X2APIC    = (1 << 21),
    CPUID_FEATURE_ECX_MOVBE     = (1 << 22),
    CPUID_FEATURE_ECX_POPCNT    = (1 << 23),
    CPUID_FEATURE_ECX_TSCDL     = (1 << 24),
    CPUID_FEATURE_ECX_AES       = (1 << 25),
    CPUID_FEATURE_ECX_XSAVE     = (1 << 26),
    CPUID_FEATURE_ECX_OSXSAVE   = (1 << 27),
    CPUID_FEATURE_ECX_AVX       = (1 << 28),
    CPUID_FEATURE_ECX_F16C      = (1 << 29),
    CPUID_FEATURE_ECX_RDRND     = (1 << 30),
    CPUID_FEATURE_ECX_HV        = (1 << 31)
} cpuid_feature_ecx;

typedef enum
{
    CPUID_FEATURE_EXT_ECX_LAHF      = (1 << 0),
    CPUID_FEATURE_EXT_ECX_LZCNT     = (1 << 5),
    CPUID_FEATURE_EXT_ECX_PREFETCHW = (1 << 8),
} cpuid_feature_ext_ecx;

typedef enum
{
    CPUID_FEATURE_EXT_EDX_SYSCALL   = (1 << 11),
    CPUID_FEATURE_EXT_EDX_NX        = (1 << 20),
    CPUID_FEATURE_EXT_EDX_GB_PAGE   = (1 << 26),
    CPUID_FEATURE_EXT_EDX_RDTSCP    = (1 << 27),
    CPUID_FEATURE_EXT_EDX_LONG_MODE = (1 << 29)
} cpuid_feature_ext_edx;

extern const cpuid_vendor* cpuid_detected_vendor;
extern uint8 cpuid_max_eax;

extern uint8 cpuid_processor_type;
extern uint16 cpuid_family_id;
extern uint8 cpuid_model_id;

extern void cpuid_init(void);

extern bool cpuid_supports_feature_edx(cpuid_feature_edx f);
extern bool cpuid_supports_feature_ecx(cpuid_feature_ecx f);
extern bool cpuid_supports_feature_ext_edx(cpuid_feature_ext_edx f);
extern bool cpuid_supports_feature_ext_ecx(cpuid_feature_ext_ecx f);

#endif
