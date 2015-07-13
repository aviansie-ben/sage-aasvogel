#ifndef MEMORY_PAGE_PAE_H
#define MEMORY_PAGE_PAE_H

#ifndef ALLOW_PAGE_H_DIRECT
#error "memory/page_pae.h SHOULD NOT be included except within memory management code! Use memory/page.h instead!"
#endif

#include <memory/page.h>

extern void kmem_page_pae_init(const boot_param* param) __hidden;
extern void kmem_page_pae_init2(const boot_param* param) __hidden;

extern bool kmem_page_pae_context_create(page_context* c, bool kernel_table) __hidden;
extern void kmem_page_pae_context_destroy(page_context* c) __hidden;

extern bool kmem_page_pae_get(page_context* c, addr_v virtual, addr_p* physical, uint64* flags) __hidden;
extern bool kmem_page_pae_set(page_context* c, addr_v virtual, addr_p physical, uint64 flags) __hidden;

#endif
