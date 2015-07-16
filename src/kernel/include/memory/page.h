#ifndef MEMORY_PAGE_H
#define MEMORY_PAGE_H

#include <typedef.h>
#include <lock/spinlock.h>
#include <memory/phys.h>

#define KERNEL_VIRTUAL_ADDRESS_BEGIN  0xC0000000u
#define PAGE_PHYSICAL_ADDRESS_MASK_64 0x7FFFFFFFFFFFF000u
#define PAGE_PHYSICAL_ADDRESS_MASK_32 0xFFFFF000u

typedef uint32 addr_v;

enum pdpt_entry_flags
{
    PDPT_ENTRY_PRESENT       = (1 << 0),
    PDPT_ENTRY_WRITE_THROUGH = (1 << 3),
    PDPT_ENTRY_CACHE_DISABLE = (1 << 4)
};

enum pd_entry_flags
{
    PD_ENTRY_PRESENT       = (1 << 0),
    PD_ENTRY_WRITEABLE     = (1 << 1),
    PD_ENTRY_USER          = (1 << 2),
    PD_ENTRY_WRITE_THROUGH = (1 << 3),
    PD_ENTRY_CACHE_DISABLE = (1 << 4),
    PD_ENTRY_ACCESSED      = (1 << 5),
    PD_ENTRY_DIRTY         = (1 << 6),
    PD_ENTRY_LARGE_PAGE    = (1 << 7),
    PD_ENTRY_GLOBAL        = (1 << 8),
    PD_ENTRY_NO_EXECUTE    = (1ull << 63),
    
    PD_ENTRY_DEF_KERNEL    = PD_ENTRY_WRITEABLE,
    PD_ENTRY_DEF_USER      = PD_ENTRY_WRITEABLE | PD_ENTRY_USER
};

enum pt_entry_flags
{
    PT_ENTRY_PRESENT       = (1 << 0),
    PT_ENTRY_WRITEABLE     = (1 << 1),
    PT_ENTRY_USER          = (1 << 2),
    PT_ENTRY_WRITE_THROUGH = (1 << 3),
    PT_ENTRY_CACHE_DISABLE = (1 << 4),
    PT_ENTRY_ACCESSED      = (1 << 5),
    PT_ENTRY_DIRTY         = (1 << 6),
    PT_ENTRY_PAT_ENABLED   = (1 << 7),
    PT_ENTRY_GLOBAL        = (1 << 8),
    PT_ENTRY_NO_EXECUTE    = (1ull << 63)
};

struct page_context;

struct page_dir_ptr_tab;

struct page_dir_legacy;
struct page_table_legacy;

typedef struct page_context
{
    spinlock lock;
    
    union
    {
        struct page_dir_ptr_tab* pae_pdpt;
        struct page_dir_legacy* legacy_dir;
    };
    
    uint32 physical_address;
    
    struct page_context* next;
    struct page_context* prev;
} page_context;

typedef struct page_dir_legacy
{
    uint32 page_table_phys[1024];
    struct page_table_legacy* page_table_virt[1024];
} page_dir_legacy __attribute__((aligned(0x1000)));

typedef struct page_table_legacy
{
    uint32 page_phys[1024];
} page_table_legacy __attribute__((aligned(0x1000)));

extern page_context kernel_page_context;
extern page_context* active_page_context;

extern bool kmem_page_pae_enabled;
extern bool kmem_page_pge_enabled;
extern addr_v kmem_page_resv_end __hidden;

void kmem_page_init(const boot_param* param) __hidden;

bool kmem_page_context_create(page_context* c) __warn_unused_result;
void kmem_page_context_destroy(page_context* c);
void kmem_page_context_switch(page_context* c);

void* kmem_page_global_alloc(uint64 page_flags, frame_alloc_flags alloc_flags, uint32 num_pages) __warn_unused_result;
void kmem_page_global_free(void* addr, uint32 num_pages);

bool kmem_page_get(page_context* c, addr_v virtual_address, addr_p* physical_address, uint64* flags) __pure __warn_unused_result;
bool kmem_page_map(page_context* c, addr_v virtual_address, uint64 flags, bool flush, addr_p frame) __warn_unused_result;
void kmem_page_unmap(page_context* c, addr_v virtual_address, bool flush);

bool kmem_page_global_get(addr_v virtual_address, addr_p* physical_address, uint64* flags) __pure __warn_unused_result;
bool kmem_page_global_map(addr_v virtual_address, uint64 flags, bool flush, addr_p frame) __warn_unused_result;
void kmem_page_global_unmap(addr_v virtual_address, bool flush);

void kmem_page_flush_one(addr_v virtual_address);
void kmem_page_flush_region(addr_v virtual_address, uint32 num_pages);
void kmem_page_flush_all(void);

void kmem_enable_write_protect(void);
void kmem_disable_write_protect(void);

#endif
