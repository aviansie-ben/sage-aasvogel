#ifndef CORE_BOOTPARAM_H
#define CORE_BOOTPARAM_H

#include <typedef.h>
#include <multiboot.h>

typedef struct {
    multiboot_info* multiboot;
    
    size_t num_cmdline_parts;
    const char** cmdline_parts;
} boot_param;

extern void parse_boot_cmdline(multiboot_info* multiboot, boot_param* param);
extern bool cmdline_get_bool(const boot_param* param, const char* param_name);

#endif
