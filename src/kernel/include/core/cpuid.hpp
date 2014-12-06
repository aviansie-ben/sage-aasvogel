#ifndef __CPUID_HPP__
#define __CPUID_HPP__

#include <typedef.hpp>

namespace cpuid
{
    struct cpu_vendor
    {
        const char* vendor_id;
        const char* vendor_name;
    };
    
    enum cpu_feature_edx
    {
        FEATURE_EDX_FPU   = (1 << 0),
        FEATURE_EDX_VME   = (1 << 1),
        FEATURE_EDX_DE    = (1 << 2),
        FEATURE_EDX_PSE   = (1 << 3),
        FEATURE_EDX_TSC   = (1 << 4),
        FEATURE_EDX_MSR   = (1 << 5),
        FEATURE_EDX_PAE   = (1 << 6),
        FEATURE_EDX_MCE   = (1 << 7),
        FEATURE_EDX_CX8   = (1 << 8),
        FEATURE_EDX_APIC  = (1 << 9),
        FEATURE_EDX_SEP   = (1 << 11),
        FEATURE_EDX_MTRR  = (1 << 12),
        FEATURE_EDX_PGE   = (1 << 13),
        FEATURE_EDX_MCA   = (1 << 14),
        FEATURE_EDX_CMOV  = (1 << 15),
        FEATURE_EDX_PAT   = (1 << 16),
        FEATURE_EDX_PSE36 = (1 << 17),
        FEATURE_EDX_PSN   = (1 << 18),
        FEATURE_EDX_CLF   = (1 << 19),
        FEATURE_EDX_DS    = (1 << 21),
        FEATURE_EDX_ACPI  = (1 << 22),
        FEATURE_EDX_MMX   = (1 << 23),
        FEATURE_EDX_FXSR  = (1 << 24),
        FEATURE_EDX_SSE   = (1 << 25),
        FEATURE_EDX_SSE2  = (1 << 26),
        FEATURE_EDX_SS    = (1 << 27),
        FEATURE_EDX_HTT   = (1 << 28),
        FEATURE_EDX_TM    = (1 << 29),
        FEATURE_EDX_IA64  = (1 << 30),
        FEATURE_EDX_PBE   = (1 << 31)
    };
    
    enum cpu_feature_ecx
    {
        FEATURE_ECX_SSE3      = (1 << 0),
        FEATURE_ECX_PCLMULQDQ = (1 << 1),
        FEATURE_ECX_DTES64    = (1 << 2),
        FEATURE_ECX_MONITOR   = (1 << 3),
        FEATURE_ECX_DSCPL     = (1 << 4),
        FEATURE_ECX_VMX       = (1 << 5),
        FEATURE_ECX_SMX       = (1 << 6),
        FEATURE_ECX_EST       = (1 << 7),
        FEATURE_ECX_TM2       = (1 << 8),
        FEATURE_ECX_SSSE3     = (1 << 9),
        FEATURE_ECX_CNXTID    = (1 << 10),
        FEATURE_ECX_SDBG      = (1 << 11),
        FEATURE_ECX_FMA       = (1 << 12),
        FEATURE_ECX_CX16      = (1 << 13),
        FEATURE_ECX_XTPR      = (1 << 14),
        FEATURE_ECX_PDCM      = (1 << 15),
        FEATURE_ECX_PCID      = (1 << 17),
        FEATURE_ECX_DCA       = (1 << 18),
        FEATURE_ECX_SSE41     = (1 << 19),
        FEATURE_ECX_SSE42     = (1 << 20),
        FEATURE_ECX_X2APIC    = (1 << 21),
        FEATURE_ECX_MOVBE     = (1 << 22),
        FEATURE_ECX_POPCNT    = (1 << 23),
        FEATURE_ECX_TSCDL     = (1 << 24),
        FEATURE_ECX_AES       = (1 << 25),
        FEATURE_ECX_XSAVE     = (1 << 26),
        FEATURE_ECX_OSXSAVE   = (1 << 27),
        FEATURE_ECX_AVX       = (1 << 28),
        FEATURE_ECX_F16C      = (1 << 29),
        FEATURE_ECX_RDRND     = (1 << 30),
        FEATURE_ECX_HV        = (1 << 31)
    };
    
    extern void init();
    
    extern const cpu_vendor* get_vendor();
    
    extern uint8 get_processor_type();
    extern uint16 get_family_id();
    extern uint8 get_model_id();
    
    extern bool supports_feature(cpu_feature_ecx f);
    extern bool supports_feature(cpu_feature_edx f);
}

#endif
