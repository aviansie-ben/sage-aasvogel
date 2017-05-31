#include <cpu/msr.h>
#include <cpu/cpuid.h>

bool msr_is_supported(void)
{
    return cpuid_supports_feature_edx(CPUID_FEATURE_EDX_MSR);
}

uint64 msr_read(uint32 msr)
{
    uint32 lo, hi;

    asm volatile ("rdmsr" : "=a" (lo), "=d" (hi) : "c" (msr));
    return (uint64)lo | ((uint64)hi << 32);
}

void msr_write(uint32 msr, uint64 val)
{
    asm volatile ("wrmsr" : : "a" ((uint32)(val & 0xffffffff)), "d" ((uint32)(val >> 32)), "c" (msr));
}
