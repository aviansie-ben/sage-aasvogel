#ifndef __PREINIT_H__
#define __PREINIT_H__

#include <typedef.h>
#include <multiboot.h>

#define PREINIT_ERROR_COLOR 0x04

#define __section__(s) __attribute__((section(s)))

typedef union
{
    uint32 legacy[1024];
    uint64 pae[512];
} _preinit_page_struct;

// We can't directly use string constants before we've mapped the kernel into
// the higher half, since string constants are in the .rodata section, which
// points to memory that hasn't been mapped yet!
extern char _preinit_serial_stop_msg[];
extern char _preinit_serial_init_msg[];
extern char _preinit_serial_page_legacy_msg[];
extern char _preinit_serial_page_pae_msg[];

extern char _preinit_cmdline_serial_debug[];
extern char _preinit_cmdline_no_pae[];

// If PAE is enabled, this will be set to true. This is used by boot.asm to
// determine whether or not to enable PAE on the CPU and by the memory manager
// later to determine whether to use PAE in the kernel proper.
extern bool _preinit_pae_enabled;

// Values for these variables are read from the command line passed in by the
// bootloader at runtime.
extern bool _preinit_serial_enable;
extern bool _preinit_no_pae;

// Generic pre-initialization functions
extern void _preinit_parse_cmdline(multiboot_info* mb_info) __hidden __section__(".setup");
extern bool _preinit_cmdline_option_set(const char* cmdline, const char* option) __hidden __section__(".setup");

extern void _preinit_error(const char* message) __hidden __attribute__((section(".setup"), noreturn));

// Functions for controlling the serial port if serial port logging was enabled
// by the bootloader.
extern void _preinit_outb(uint16 port, uint8 data) __hidden __section__(".setup");
extern uint8 _preinit_inb(uint16 port) __hidden __section__(".setup");

extern void _preinit_setup_serial(void) __hidden __section__(".setup");
extern void _preinit_write_serial(const char* message) __hidden __section__(".setup");

// Functions for setting up paging
extern void* _preinit_setup_paging(bool pae_supported) __hidden __section__(".setup");
extern void* _preinit_pae_setup_paging(void) __hidden __section__(".setup");
extern void* _preinit_legacy_setup_paging(void) __hidden __section__(".setup");

#endif
