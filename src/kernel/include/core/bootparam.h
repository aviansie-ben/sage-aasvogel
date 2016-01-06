#ifndef CORE_BOOTPARAM_H
#define CORE_BOOTPARAM_H

#include <typedef.h>
#include <multiboot.h>

typedef struct {
    multiboot_info* multiboot;
    
    size_t num_cmdline_parts;
    const char** cmdline_parts;
} boot_param;

extern void parse_boot_cmdline(multiboot_info* multiboot, boot_param* param) __hidden;
extern bool cmdline_get_bool(const boot_param* param, const char* param_name) __pure;
extern const char* cmdline_get_str(const boot_param* param, const char* param_name, const char* def_value) __pure;
extern int32 cmdline_get_int(const boot_param* param, const char* param_name, int32 min_value, int32 max_value, int32 def_value);

#endif
