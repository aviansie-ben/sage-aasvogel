#include <core/cpuid.hpp>
#include <string.hpp>

namespace cpuid
{
    struct eax_processor_info
    {
        uint8 stepping : 4;
        uint8 model : 4;
        uint8 family : 4;
        uint8 processor_type : 4;
        uint8 ext_model : 4;
        uint8 ext_family;
        uint8 reserved : 4;
    } __attribute__((packed));
    
    struct ebx_processor_info
    {
        uint8 apic_id;
        uint8 clflush_size;
        uint8 reserved;
        uint8 brand_index;
    } __attribute__((packed));
    
    static char vendor_id[13];
    static cpu_vendor unknown_vendor = { vendor_id, "Unknown Vendor" };
    
    static cpu_vendor known_vendors[] = {
        { "AMDisbetter!", "AMD" },
        { "AuthenticAMD", "AMD" },
        { "GenuineIntel", "Intel" },
        { "VIA VIA VIA ", "VIA Technologies" },
        { "CentaurHauls", "Centaur Technology" },
        { "TransmetaCPU", "Transmeta" },
        { "GenuineTMx86", "Transmeta" },
        { "CyrixInstead", "Cyrix" },
        { "NexGenDriven", "NexGen" },
        { "UMC UMC UMC ", "United Microelectrics Corporation" },
        { "SiS SiS SiS ", "Silicon Integrated Systems" },
        { "Geode by NSC", "National Semiconductor" },
        { "RiseRiseRise", "Rise Technology" },
        { "Vortex86 SoC", "Vortex86" },
        { "KVMKVMKVMKVM", "Kernel-based Virtual Machine" },
        { "Microsoft Hv", "Hyper-V Virtual Machine" },
        { "VMwareVMware", "VMware Virtual Machine" },
        { "XenVMMXenVMM", "Xen Virtual Machine" }
    };
    
    static cpu_vendor* vendor;
    static uint32 max_eax;
    
    static union
    {
        eax_processor_info info;
        uint32 val;
    } info_eax;
    
    static union
    {
        ebx_processor_info info;
        uint32 val;
    } info_ebx;
    static uint32 features_ecx;
    static uint32 features_edx;
    
    void init()
    {
        uint32* vendor_id_regs = (uint32*) vendor_id;
        
        // Run CPUID with EAX=0 to get the maximum EAX value and the vendor id
        asm volatile ("cpuid" : "=b" (vendor_id_regs[0]), "=d" (vendor_id_regs[1]), "=c" (vendor_id_regs[2]), "=a" (max_eax) : "a" (0));
        
        // Attempt to match the vendor id to a known vendor id
        vendor = &unknown_vendor;
        
        for (size_t i = 0; i < (sizeof(known_vendors) / sizeof(*known_vendors)); i++)
        {
            if (!strcmp(vendor_id, known_vendors[i].vendor_id))
            {
                vendor = &known_vendors[i];
                break;
            }
        }
        
        // Run CPUID with EAX=1 to get processor info and featureset
        asm volatile ("cpuid" : "=a" (info_eax.val), "=b" (info_ebx.val), "=c" (features_ecx), "=d" (features_edx) : "a" (1));
    }
    
    const cpu_vendor* get_vendor()
    {
        return vendor;
    }
    
    uint8 get_processor_type()
    {
        return info_eax.info.processor_type;
    }
    
    uint16 get_family_id()
    {
        // When the family ID is 0xF, we need to take the extended family ID into account
        if (info_eax.info.family == 0xF)
        {
            return info_eax.info.family + info_eax.info.ext_family;
        }
        else
        {
            return info_eax.info.family;
        }
    }
    
    uint8 get_model_id()
    {
        // When the family ID is 0x6 or 0xF, we need to take the extended model ID into account
        if (info_eax.info.family == 0x6 || info_eax.info.family == 0xF)
        {
            return info_eax.info.model + (info_eax.info.ext_model << 4);
        }
        else
        {
            return info_eax.info.model;
        }
    }
    
    bool supports_feature(cpu_feature_ecx f)
    {
        return (features_ecx & f) != 0;
    }
    
    bool supports_feature(cpu_feature_edx f)
    {
        return (features_edx & f) != 0;
    }
}
