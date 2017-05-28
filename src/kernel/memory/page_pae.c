#include <memory/early.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <memory/pool.h>

#include <core/cpuid.h>
#include <core/msr.h>

#include <core/crash.h>
#include <assert.h>

#define ALLOW_PAGE_H_DIRECT
#include <memory/page_pae.h>

#define PDPT_SIZE 4
#define PDPT_SHIFT 30
#define PDPT_MASK -(1u << PDPT_SHIFT)

#define PD_SIZE 512
#define PD_SHIFT 21
#define PD_MASK (-(1u << PD_SHIFT) & ~PDPT_MASK)

#define PT_SIZE 512
#define PT_SHIFT FRAME_SHIFT
#define PT_MASK (-(1u << PT_SHIFT) & ~PDPT_MASK & ~PD_MASK)

typedef struct page_dir_pae
{
    uint64 page_table_phys[PD_SIZE];
} page_dir_pae __attribute__((aligned(FRAME_SIZE)));

typedef struct page_table_pae
{
    uint64 page_phys[PT_SIZE];
} page_table_pae __attribute__((aligned(FRAME_SIZE)));

typedef struct page_dir_ptr_tab
{
    uint64 page_dir_phys[PDPT_SIZE];
    struct page_dir_pae* page_dir_virt[PDPT_SIZE];

    struct page_table_pae** page_table_virt;
    bool kernel;
} page_dir_ptr_tab __attribute__((aligned(0x20)));

static bool init2_done;
static bool use_nx = false;

static addr_v global_tables;

static page_dir_ptr_tab pdpt_global;
static mempool_small pdpt_pool;

static page_table_pae* _get_page_table(page_dir_ptr_tab* pdpt, addr_v address)
{
    return pdpt->page_table_virt[address >> PD_SHIFT];
}

static bool _get_entry(page_dir_ptr_tab* pdpt, addr_v address, addr_p* phys, uint64* flags)
{
    page_table_pae* t = _get_page_table(pdpt, address);
    uint32 pte = (address & PT_MASK) >> PT_SHIFT;

    if (t == NULL || t->page_phys[pte] == 0)
        return false;

    if (phys != NULL) *phys = (t->page_phys[pte] & PAGE_PHYSICAL_ADDRESS_MASK_64) + (address & FRAME_OFFSET_MASK);
    if (flags != NULL) *flags = (t->page_phys[pte] & ~PAGE_PHYSICAL_ADDRESS_MASK_64);

    return true;
}

static page_table_pae* _alloc_page_table(page_dir_ptr_tab* pdpt, addr_v address)
{
    uint32 pdpte = (address & PDPT_MASK) >> PDPT_SHIFT;
    uint32 pde = (address & PD_MASK) >> PD_SHIFT;
    uint32 pdet = (address & (PDPT_MASK | PD_MASK)) >> PD_SHIFT;
    addr_p frame;
    page_table_pae* pt;

    if (pdpt->page_dir_virt[pdpte] == NULL)
        crash("Attempt to map a page into a reserved area!");

    frame = kmem_frame_alloc(0);
    if (frame == FRAME_NULL)
        return NULL;

    pt = (page_table_pae*)kmem_virt_alloc(1);

    if (pt == NULL)
    {
        kmem_frame_free(frame);
        return NULL;
    }

    pdpt->page_dir_virt[pdpte]->page_table_phys[pde] = frame | PD_ENTRY_PRESENT | PD_ENTRY_WRITEABLE | PD_ENTRY_USER;

    return pdpt->page_table_virt[pdet] = pt;
}

static page_table_pae* _alloc_page_table_global(page_dir_ptr_tab* pdpt, addr_v address)
{
    uint32 pdpte = (address & PDPT_MASK) >> PDPT_SHIFT;
    uint32 pde = (address & PD_MASK) >> PD_SHIFT;
    uint32 pdet = (address & (PDPT_MASK | PD_MASK)) >> PD_SHIFT;

    if (pdpt->page_dir_virt[pdpte] == NULL)
        crash("Attempt to map a page into a reserved area!");

    if (init2_done)
    {
        addr_p frame = kmem_frame_alloc(FA_EMERG);
        if (frame == FRAME_NULL)
            return NULL;

        kmem_page_pae_set(&kernel_page_context, global_tables + (pde * FRAME_SIZE), frame, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL | PT_ENTRY_PRESENT);
        kmem_page_flush_one(global_tables + (pde * FRAME_SIZE));

        pdpt->page_dir_virt[pdpte]->page_table_phys[pde] = frame | PD_ENTRY_PRESENT | PD_ENTRY_WRITEABLE;
        if (kmem_page_pge_enabled)
            pdpt->page_dir_virt[pdpte]->page_table_phys[pde] |= PD_ENTRY_GLOBAL;

        pdpt->page_table_virt[pdet] = (page_table_pae*)(global_tables + (pde * FRAME_SIZE));

        for (size_t i = 0; i < PT_SIZE; i++)
            pdpt->page_table_virt[pdet]->page_phys[i] = 0;
    }
    else
    {
        uint32 pde_map_begin = ((kmem_early_next_alloc - global_tables) & PD_MASK) >> PD_SHIFT;
        assert(kmem_early_next_alloc <= global_tables + (pde * FRAME_SIZE));

        for (uint32 pde_map = pde_map_begin; pde_map <= pde; pde_map++)
        {
            page_table_pae* pt = (page_table_pae*) (global_tables + pde_map * FRAME_SIZE);

            pdpt->page_table_virt[pde_map + pdpte * PD_SIZE] = pt;
            pdpt->page_dir_virt[pdpte]->page_table_phys[pde_map] = ((uint32)pt - 0xC0000000) | PD_ENTRY_PRESENT | PD_ENTRY_WRITEABLE;

            for (size_t i = 0; i < PT_SIZE; i++)
                pdpt->page_table_virt[pdet]->page_phys[i] = 0;
        }

        kmem_early_next_alloc = global_tables + ((pde + 1) * FRAME_SIZE);

        for (uint32 pde_map = pde_map_begin; pde_map <= pde; pde_map++)
        {
            page_table_pae* pt = (page_table_pae*) (global_tables + pde_map * FRAME_SIZE);
            kmem_page_pae_set(&kernel_page_context, (uint32)pt, (uint32)pt - 0xC0000000, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL | PT_ENTRY_PRESENT);
        }
    }

    return pdpt->page_table_virt[pdet];
}

void kmem_page_pae_init(const boot_param* param)
{
    uint32 a;

    // Intialize the PDPT pool (It cannot be used yet, but it can be initialized
    // before the memory manager is fully ready)
    kmem_pool_small_init(&pdpt_pool, "page_dir_ptr_tab pool", sizeof(page_dir_ptr_tab), __alignof__(page_dir_ptr_tab), FA_32BIT);

    // If the CPU supports the NX bit, we should enable it now
    use_nx = !cmdline_get_bool(param, "no_nx") && msr_is_supported() && cpuid_supports_feature_ext_edx(CPUID_FEATURE_EXT_EDX_NX);
    if (use_nx)
        msr_write(MSR_EFER, msr_read(MSR_EFER) | MSR_EFER_FLAG_NX);

    // Initialize the global PDPT before the early memory manager is finalized.
    pdpt_global.page_dir_phys[0] = pdpt_global.page_dir_phys[1] = pdpt_global.page_dir_phys[2] = 0;
    pdpt_global.page_dir_virt[0] = pdpt_global.page_dir_virt[1] = pdpt_global.page_dir_virt[2] = NULL;

    pdpt_global.page_dir_virt[3] = kmalloc_early(sizeof(page_dir_pae), __alignof__(page_dir_pae), &a);
    pdpt_global.page_dir_phys[3] = a;
    pdpt_global.page_dir_phys[3] |= PDPT_ENTRY_PRESENT;

    pdpt_global.page_table_virt = kmalloc_early(sizeof(page_table_pae*) * (PD_SIZE * PDPT_SIZE), __alignof__(page_table_pae*), NULL);

    // Finalize the early memory manager to ensure that no more allocations are
    // made now that initialization has begun.
    kmem_early_finalize();

    // The early memory manager is finalized now, so we can put our reserved
    // virtual addresses right after the end of the early allocations.
    global_tables = kmem_early_next_alloc;

    if ((global_tables & (FRAME_SIZE - 1)) != 0)
    {
        global_tables |= (FRAME_SIZE - 1);
        global_tables += 1;
    }

    kmem_page_resv_end = global_tables + (PD_SIZE * FRAME_SIZE);
}

void kmem_page_pae_init2(const boot_param* param)
{
    // Ensure that page tables are allocated beforehand for storing global
    // tables.
    for (addr_v addr = global_tables; addr < kmem_page_resv_end; addr += FRAME_SIZE)
        kmem_page_pae_set(&kernel_page_context, addr, 0, 0);

    init2_done = true;
}

bool kmem_page_pae_context_create(page_context* c, bool kernel_table)
{
    size_t i;
    size_t j;

    if (kernel_table)
    {
        c->pae_pdpt = &pdpt_global;
        c->physical_address = (uint32)&pdpt_global - 0xC0000000;
    }
    else
    {
        addr_p phys;

        c->pae_pdpt = kmem_pool_small_alloc(&pdpt_pool, 0);
        if (c->pae_pdpt == NULL) return false;

        _get_entry(kernel_page_context.pae_pdpt, (addr_v) c->pae_pdpt, &phys, NULL);
        c->physical_address = (uint32)phys;

        for (i = 0; i < PDPT_SIZE - 1; i++)
        {
            c->pae_pdpt->page_dir_virt[i] = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, 0, 1);

            if (c->pae_pdpt->page_dir_virt[i] == NULL)
            {
                for (j = i; j > 0; j--)
                    kmem_page_global_free(c->pae_pdpt->page_dir_virt[j], 1);

                kmem_pool_small_free(&pdpt_pool, c->pae_pdpt);
                return false;
            }

            _get_entry(kernel_page_context.pae_pdpt, (addr_v) c->pae_pdpt->page_dir_virt[i], &c->pae_pdpt->page_dir_phys[i], NULL);
            c->pae_pdpt->page_dir_phys[i] |= PDPT_ENTRY_PRESENT;
        }

        // We want to avoid accidentally mapping user pages into the upper part
        // of the address space, so we leave the virtual address of the page
        // directory as NULL.
        c->pae_pdpt->page_dir_virt[3] = NULL;
        c->pae_pdpt->page_dir_phys[3] = kernel_page_context.pae_pdpt->page_dir_phys[3];

        if ((c->pae_pdpt->page_table_virt = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, 0, (sizeof(page_table_pae*) * (PD_SIZE * PDPT_SIZE)))) == NULL)
        {
            for (j = PDPT_SIZE - 1; j > 0; j--)
                kmem_page_global_free(c->pae_pdpt->page_dir_virt[j], 1);

            kmem_pool_small_free(&pdpt_pool, c->pae_pdpt);

            return false;
        }
    }

    c->pae_pdpt->kernel = kernel_table;
    for (i = 0; i < PD_SIZE * PDPT_SIZE; i++)
        c->pae_pdpt->page_table_virt[i] = NULL;

    return true;
}

void kmem_page_pae_context_destroy(page_context* c)
{
    size_t i;

    if (c->pae_pdpt->kernel)
        crash("Attempt to destroy kernel page context!");

    // Free all the page tables that were allocated to this context
    for (i = 0; i < PDPT_SIZE * PD_SIZE; i++)
    {
        if (c->pae_pdpt->page_table_virt[i] != NULL)
            kmem_page_global_free(c->pae_pdpt->page_table_virt[i], 1);
    }

    // Free the space that stores virtual page table addresses
    kmem_page_global_free(c->pae_pdpt->page_table_virt, (sizeof(page_table_pae*) * (PD_SIZE * PDPT_SIZE)));
    c->pae_pdpt->page_table_virt = NULL;

    // Free all the page directories and set their entries to a reserved value
    for (i = 0; i < PDPT_SIZE; i++)
    {
        if (c->pae_pdpt->page_dir_virt[i] != NULL)
        {
            kmem_page_global_free(c->pae_pdpt->page_dir_virt[i], 1);
            c->pae_pdpt->page_dir_virt[i] = NULL;
        }

        c->pae_pdpt->page_dir_phys[i] = ~((uint64)PDPT_ENTRY_PRESENT);
    }

    // Finally, free the PDPT
    kmem_pool_small_free(&pdpt_pool, c->pae_pdpt);
    c->pae_pdpt = NULL;
    c->physical_address = 0;
}

bool kmem_page_pae_get(page_context* c, addr_v virtual, addr_p* physical, uint64* flags)
{
    addr_p tphys;
    uint64 tflags;

    if (_get_entry(c->pae_pdpt, virtual & PAGE_MASK, &tphys, &tflags))
    {
        if ((tflags & PT_ENTRY_PRESENT) == 0) return false;

        if (physical != NULL) *physical = tphys;
        if (flags != NULL) *flags = tflags;

        return true;
    }
    else
    {
        return false;
    }
}

bool kmem_page_pae_set(page_context* c, addr_v virtual, addr_p physical, uint64 flags)
{
    page_table_pae* t;

    if (!use_nx)
        flags &= ~PT_ENTRY_NO_EXECUTE;

    if (!kmem_page_pge_enabled)
        flags &= (uint64)~PT_ENTRY_GLOBAL;

    if ((t = _get_page_table(c->pae_pdpt, virtual)) == NULL)
    {
        if ((t = ((c->pae_pdpt->kernel) ? _alloc_page_table_global(c->pae_pdpt, virtual) : _alloc_page_table(c->pae_pdpt, virtual))) == NULL)
            return false;
    }

    t->page_phys[(virtual & PT_MASK) >> PT_SHIFT] = physical | flags;
    return true;
}
