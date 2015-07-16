#ifndef MEMORY_EARLY_H
#define MEMORY_EARLY_H

#include <typedef.h>
#include <multiboot.h>
#include <memory/page.h>

extern addr_v kmem_early_next_alloc __hidden;
extern addr_v kmem_early_min_alloc __hidden;

extern void* kmalloc_early(size_t size, size_t align, uint32* physical_address) __hidden __warn_unused_result;

extern void kmem_early_init(multiboot_info* multiboot) __hidden;
extern void kmem_early_finalize(void) __hidden;

#endif
