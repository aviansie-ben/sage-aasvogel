#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <typedef.h>

typedef enum
{
    MB_FLAG_MEMORY      = (1 << 0),
    MB_FLAG_BOOTDEV     = (1 << 1),
    MB_FLAG_CMDLINE     = (1 << 2),
    MB_FLAG_MODULES     = (1 << 3),
    MB_FLAG_AOUT_SYMS   = (1 << 4),
    MB_FLAG_ELF_SHDR    = (1 << 5),
    MB_FLAG_MEM_MAP     = (1 << 6),
    MB_FLAG_DRIVE_INFO  = (1 << 7),
    MB_FLAG_CONFIG_TBL  = (1 << 8),
    MB_FLAG_LOADER_NAME = (1 << 9),
    MB_FLAG_APM_TABLE   = (1 << 10),
    MB_FLAG_VIDEO_INFO  = (1 << 11),
} multiboot_flag;

typedef struct
{
    uint32 num;
    uint32 size;
    uint32 addr;
    uint32 shndx;
} multiboot_elf_shdr_table;

typedef struct
{
    uint32 flags;

    // Basic memory information. Requires flag MB_FLAG_MEMORY.
    uint32 mem_lower;
    uint32 mem_upper;

    // Boot device information. Requires flag MB_FLAG_BOOTDEV.
    uint8 bootdev_part1;
    uint8 bootdev_part2;
    uint8 bootdev_part3;
    uint8 bootdev_drive;

    // Command line used to start kernel. Requires flag MB_FLAG_CMDLINE.
    uint32 cmdline_addr;

    // Module information table. Requires flag MB_FLAG_MODULES.
    uint32 mods_count;
    uint32 mods_addr;

    // ELF symbol header table. Requires flag MB_FLAG_ELF_SHDR.
    multiboot_elf_shdr_table elf_shdr_table;

    // Memory map. Requires flag MB_FLAG_MEM_MAP.
    uint32 mmap_length;
    uint32 mmap_addr;

    // Drive information table. Requires flag MB_FLAG_DRIVE_INFO.
    uint32 drives_length;
    uint32 drives_addr;

    // ROM configuration table. Requires flag MB_FLAG_CONFIG_TBL.
    uint32 config_table;

    // Boot loader name. Requires flag MB_FLAG_LOADER_NAME.
    uint32 boot_loader_name;

    // APM table. Requires flag MB_FLAG_APM_TABLE.
    uint32 apm_table;

    // Graphics information. Requires flag MB_FLAG_VIDEO_INFO.
    uint32 vbe_control_info;
    uint32 vbe_mode_info;
    uint32 vbe_mode;
    uint32 vbe_interface_seg;
    uint32 vbe_interface_off;
    uint32 vbe_interface_len;
} multiboot_info;

typedef struct
{
    // Module is loaded from mod_start to mod_end - 1.
    uint32 mod_start;
    uint32 mod_end;

    // The name associated with the given module in the bootloader config.
    uint32 name;

    // Unused bytes
    uint32 reserved;
} multiboot_module_entry;

typedef struct
{
    // The size of this entry (not counting this field)
    uint32 size;

    // The address and length of this memory section.
    uint64 base_addr;
    uint64 length;

    // 1 indicates available RAM. All other values indicate a reserved area.
    uint32 type;
} multiboot_mmap_entry;

#endif
