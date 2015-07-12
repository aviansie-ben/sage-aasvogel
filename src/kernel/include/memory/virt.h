#ifndef MEMORY_VIRT_H
#define MEMORY_VIRT_H

#include <memory/page.h>

extern void kmem_virt_init(const boot_param* param);

extern void* kmem_virt_alloc(uint32 num_pages);
extern void kmem_virt_free(void* addr, uint32 num_pages);

#endif
