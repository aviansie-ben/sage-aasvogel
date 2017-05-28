#include <memory/virt.h>
#include <memory/phys.h>
#include <memory/early.h>

#include <core/klog.h>
#include <core/crash.h>
#include <assert.h>
#include <lock/spinlock.h>

#define FREE_REGIONS_PER_FRAME (FRAME_SIZE / sizeof(struct free_region))

typedef struct free_region
{
    addr_v address;
    size_t size;

    struct free_region* next_addr;
    struct free_region* next_size;
} free_region;

typedef struct free_free_region
{
    struct free_free_region* next;
    uint8 pad[sizeof(free_region) - sizeof(struct free_free_region*)];
} free_free_region;

typedef struct fr_frame
{
    union
    {
        free_region regions[FREE_REGIONS_PER_FRAME];
        free_free_region regions_free[FREE_REGIONS_PER_FRAME];
    };
} fr_frame;

extern const void _ld_setup_begin;
extern const void _ld_setup_end;

static spinlock fr_lock;

static free_free_region* first_free_fr;

static free_region* first_fr_addr;
static free_region* first_fr_size;

static void _create_fr_frame(addr_v addr)
{
    addr_p pframe;
    fr_frame* frame = (fr_frame*) addr;
    size_t i;

    if ((pframe = kmem_frame_alloc(FA_EMERG)) == FRAME_NULL)
        crash("Cannot allocate space for free virtual regions!");

    if (!kmem_page_global_map(addr, PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, true, pframe))
        crash("Cannot allocate space for free virtual regions!");

    for (i = 1; i < FREE_REGIONS_PER_FRAME; i++)
        frame->regions_free[i - 1].next = &frame->regions_free[i];

    frame->regions_free[FREE_REGIONS_PER_FRAME - 1].next = first_free_fr;
    first_free_fr = &frame->regions_free[0];
}

static void _region_remove_addr(free_region* r)
{
    free_region* pr = first_fr_addr;

    if (pr  == r) pr = NULL;

    for (; pr != NULL && pr->next_addr != r; pr = pr->next_addr) ;

    if (pr == NULL) first_fr_addr = r->next_addr;
    else pr->next_addr = r->next_addr;
}

static void _region_remove_size(free_region* r)
{
    free_region* pr = first_fr_size;

    if (pr  == r) pr = NULL;

    for (; pr != NULL && pr->next_size != r; pr = pr->next_size) ;

    if (pr == NULL) first_fr_size = r->next_size;
    else pr->next_size = r->next_size;
}

static void _region_insert_size(free_region* r)
{
    free_region* pr = first_fr_size;

    if (pr != NULL && pr->size <= r->size) pr = NULL;

    for (; pr != NULL && pr->next_size != NULL && pr->next_size->size > r->size; pr = pr->next_size) ;

    if (pr == NULL)
    {
        r->next_size = first_fr_size;
        first_fr_size = r;
    }
    else
    {
        r->next_size = pr->next_size;
        pr->next_size = r;
    }
}

static addr_v _alloc_region(uint32 size)
{
    free_region* r;
    free_region* pr = NULL;
    free_free_region* fr;
    addr_v addr;

    for (r = first_fr_size; r != NULL && r->next_size != NULL && r->next_size->size > size; pr = r, r = r->next_size) ;

    if (r == NULL)
        return 0;

    addr = r->address;

    if (r->size == size)
    {
        if (pr == NULL) first_fr_size = r->next_size;
        else pr->next_size = r->next_size;

        _region_remove_addr(r);

        fr = (free_free_region*) r;
        fr->next = first_free_fr;
        first_free_fr = fr;
    }
    else
    {
        r->address += size * FRAME_SIZE;
        r->size -= size;

        if (pr == NULL) first_fr_size = r->next_size;
        else pr->next_size = r->next_size;

        _region_insert_size(r);
    }

    return addr;
}

static void _free_region(addr_v addr, uint32 size)
{
    free_region* r = first_fr_addr;
    free_region* nr;
    free_free_region* fr;

    if (r != NULL && addr < r->address) r = NULL;

    for (; r != NULL && r->next_addr != NULL && r->next_addr->address <= addr; r = r->next_addr) ;

    if (r != NULL && addr < r->address + (r->size * FRAME_SIZE))
    {
        crash("Region freed when it is already free!");
    }
    else if (r != NULL && addr == r->address + (r->size * FRAME_SIZE))
    {
        r->size += size;

        if (r->next_addr != NULL && r->next_addr->address == addr + (size * FRAME_SIZE))
        {
            fr = (free_free_region*) r->next_addr;

            r->size += r->next_addr->size;

            _region_remove_size(r->next_addr);
            r->next_addr = r->next_addr->next_addr;

            fr->next = first_free_fr;
            first_free_fr = fr;
        }

        _region_remove_size(r);
        _region_insert_size(r);
    }
    else if (r != NULL && r->next_addr != NULL && r->next_addr->address == addr + (size * FRAME_SIZE))
    {
        r->next_addr->address = addr;
        r->next_addr->size += size;

        _region_remove_size(r->next_addr);
        _region_insert_size(r->next_addr);
    }
    else if (r == NULL && first_fr_addr != NULL && first_fr_addr->address == addr + (size * FRAME_SIZE))
    {
        first_fr_addr->address = addr;
        first_fr_addr->size += size;

        _region_remove_size(first_fr_addr);
        _region_insert_size(first_fr_addr);
    }
    else
    {
        if (first_free_fr == NULL)
        {
            _create_fr_frame(addr);

            addr += FRAME_SIZE;
            size -= 1;
        }

        if (size != 0)
        {
            nr = (free_region*) first_free_fr;
            first_free_fr = first_free_fr->next;

            nr->address = addr;
            nr->size = size;

            if (r == NULL)
            {
                nr->next_addr = first_fr_addr;
                first_fr_addr = nr;
            }
            else
            {
                nr->next_addr = r->next_addr;
                r->next_addr = nr;
            }

            _region_insert_size(nr);
        }
    }
}

void kmem_virt_init(const boot_param* param)
{
    addr_v alloc_end = kmem_page_resv_end;

    _free_region(alloc_end, -alloc_end / FRAME_SIZE);
    _free_region((addr_v) &_ld_setup_begin + 0xC0000000, (((addr_v) &_ld_setup_end) - ((addr_v) &_ld_setup_begin)) / FRAME_SIZE);
}

void* kmem_virt_alloc(uint32 num_pages)
{
    void* addr;

    spinlock_acquire(&fr_lock);
    addr = (void*) _alloc_region(num_pages);
    spinlock_release(&fr_lock);

    return addr;
}

void kmem_virt_free(void* addr, uint32 num_pages)
{
    assert((((addr_v)addr) & (FRAME_SIZE - 1)) == 0);

    spinlock_acquire(&fr_lock);
    _free_region((addr_v)addr, num_pages);
    spinlock_release(&fr_lock);
}
