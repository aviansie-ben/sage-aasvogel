#ifndef CORE_BOOTPARAM_H
#define CORE_BOOTPARAM_H

#include <typedef.h>
#include <multiboot.h>

typedef struct {
    const char* name;

    uint64 start_address;
    uint64 end_address;
} boot_param_module_info;

typedef struct {
    uint32 type;

    uint64 start_address;
    uint64 end_address;
} boot_param_mmap_region;

typedef struct {
    size_t num_cmdline_parts;
    const char** cmdline_parts;

    size_t num_modules;
    const boot_param_module_info* modules;

    size_t num_mmap_regions;
    const boot_param_mmap_region* mmap_regions;
} boot_param;

extern void boot_param_init(boot_param* param, const multiboot_info* multiboot) __hidden;

extern const boot_param_module_info* boot_param_find_module(const boot_param* param, const char* name);

extern bool cmdline_get_bool(const boot_param* param, const char* param_name) __pure;
extern const char* cmdline_get_str(const boot_param* param, const char* param_name, const char* def_value) __pure;
extern int32 cmdline_get_int(const boot_param* param, const char* param_name, int32 min_value, int32 max_value, int32 def_value) __pure;

#endif
