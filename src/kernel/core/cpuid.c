#include <typedef.h>
#include <core/cpuid.h>

typedef union
{
    struct
    {
        uint8 stepping : 4;
        uint8 model : 4;
        uint8 family : 4;
        uint8 processor_type : 4;
        uint8 ext_model : 4;
        uint8 ext_family;
        uint8 reserved : 4;
    } __attribute__((packed)) info;
    uint32 val;
} eax_processor_info;

typedef union
{
    struct
    {
        uint8 apic_id;
        uint8 clflush_size;
        uint8 reserved;
        uint8 brand_index;
    } __attribute__((packed)) info;
    uint32 val;
} ebx_processor_info;

static cpuid_vendor unknown_vendor = { { "" }, "Unknown Vendor" };

static const cpuid_vendor known_vendors[] = {
    { { "AMDisbetter!" }, "AMD" },
    { { "AuthenticAMD" }, "AMD" },
    { { "GenuineIntel" }, "Intel" },
    { { "VIA VIA VIA " }, "VIA Technologies" },
    { { "CentaurHauls" }, "Centaur Technologies" },
    { { "TransmetaCPU" }, "Transmeta" },
    { { "GenuineTMx86" }, "Transmeta" },
    { { "CyrixInstead" }, "Cyrix" },
    { { "NexGenDriven" }, "NexGen" },
    { { "UMC UMC UMC " }, "United Microelectrics Corporation" },
    { { "SiS SiS SiS " }, "Silicon Integrated Systems" },
    { { "Geode by NSC" }, "National Semiconductor" },
    { { "RiseRiseRise" }, "Rise Technology" },
    { { "Vortex86 SoC" }, "Vortex86" },
    { { "KVMKVMKVMKVM" }, "Kernel-based Virtual Machine" },
    { { "Microsoft Hv" }, "Hyper-V Virtual Machine" },
    { { "VMwareVMware" }, "Xen Virtual Machine" }
};

const cpuid_vendor* cpuid_detected_vendor;
uint8 cpuid_max_eax;

uint8 cpuid_processor_type;
uint16 cpuid_family_id;
uint8 cpuid_model_id;

static uint32 cpuid_features_edx;
static uint32 cpuid_features_ecx;

void cpuid_init(void)
{
    size_t i;
    eax_processor_info eax_info;
    ebx_processor_info ebx_info;
    uint32* vendor_regs = unknown_vendor.vendor_id.regs;
    
    // Run CPUID with EAX=0 to get the maximum EAX value and vendor id
    asm volatile ("cpuid" : "=b" (vendor_regs[0]), "=d" (vendor_regs[1]), "=c" (vendor_regs[2]), "=a" (cpuid_max_eax) : "a"(0));
    unknown_vendor.vendor_id.str[12] = '\0';
    
    // Match the vendor to a known vendor
    cpuid_detected_vendor = &unknown_vendor;
    
    for (i = 0; i < (sizeof(known_vendors) / sizeof(*known_vendors)); i++)
    {
        if (vendor_regs[0] == known_vendors[i].vendor_id.regs[0] && vendor_regs[1] == known_vendors[i].vendor_id.regs[1] && vendor_regs[2] == known_vendors[i].vendor_id.regs[2])
        {
            cpuid_detected_vendor = &known_vendors[i];
            break;
        }
    }
    
    // Run CPUID with EAX=1 to get processor info and featureset
    asm volatile ("cpuid" : "=a" (eax_info.val), "=b" (ebx_info.val), "=c" (cpuid_features_ecx), "=d" (cpuid_features_edx) : "a"(1));
    
    // Parse the received processor info
    cpuid_processor_type = eax_info.info.processor_type;
    cpuid_family_id = (eax_info.info.family == 0xF) ? (uint16)(0xF + eax_info.info.ext_family) : eax_info.info.family;
    cpuid_model_id = (eax_info.info.family == 0x6 || eax_info.info.family == 0xF) ? (uint8)(eax_info.info.model + (eax_info.info.ext_model << 4)) : eax_info.info.model;
}

bool cpuid_supports_feature_edx(cpuid_feature_edx f)
{
    return (cpuid_features_edx & (uint32)f) == (uint32)f;
}

bool cpuid_supports_feature_ecx(cpuid_feature_ecx f)
{
    return (cpuid_features_ecx & (uint32)f) == (uint32)f;
}
