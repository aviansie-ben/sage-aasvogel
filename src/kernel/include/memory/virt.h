#ifndef MEMORY_VIRT_H
#define MEMORY_VIRT_H

#include <memory/page.h>

extern void kmem_virt_init(const boot_param* param) __hidden;

extern void* kmem_virt_alloc(uint32 num_pages) __warn_unused_result;
extern void kmem_virt_free(void* addr, uint32 num_pages);

#endif
