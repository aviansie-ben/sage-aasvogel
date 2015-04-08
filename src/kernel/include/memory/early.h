#ifndef MEMORY_EARLY_H
#define MEMORY_EARLY_H

#include <typedef.h>
#include <multiboot.h>

extern void* kmalloc_early(size_t size, size_t align, uint32* physical_address);

extern void kmem_early_init(multiboot_info* multiboot);
extern void kmem_early_finalize(void);

#endif