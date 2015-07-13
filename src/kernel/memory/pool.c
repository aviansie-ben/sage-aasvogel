#include <memory/pool.h>
#include <memory/phys.h>
#include <memory/virt.h>

#include <core/crash.h>

typedef struct mempool_fixed_free_slot
{
    struct mempool_fixed_free_slot* next;
} mempool_fixed_free_slot;

struct mempool_small_part
{
    mempool_small* pool;
    mempool_small_part* next_part;
    
    uint32 num_free;
    mempool_fixed_free_slot* first_free;
};

static spinlock small_pool_list_lock;
static mempool_small* small_pool_list;

static void _small_pool_part_alloc(mempool_small* pool, frame_alloc_flags flags)
{
    mempool_small_part* p;
    mempool_fixed_free_slot* s;
    
    size_t i;
    
    p = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, flags, pool->frames_per_part);
    
    if (p == NULL)
        return;
    
    p->pool = pool;
    p->next_part = pool->parts_empty;
    pool->parts_empty = p;
    
    p->num_free = (pool->frames_per_part * FRAME_SIZE - sizeof(mempool_small_part) - pool->part_first_offset) / pool->obj_size;
    pool->num_total += p->num_free;
    pool->num_free += p->num_free;
    
    p->first_free = s = (mempool_fixed_free_slot*) ((addr_v)p + sizeof(mempool_small_part) + pool->part_first_offset);
    
    for (i = 0; i < p->num_free - 1; i++, s = (mempool_fixed_free_slot*) ((addr_v)s + pool->obj_size))
    {
        s->next = (mempool_fixed_free_slot*) ((addr_v)s + pool->obj_size);
    }
    
    s->next = NULL;
}

static void _small_pool_part_free(mempool_small* pool, mempool_small_part* part, mempool_small_part* prev_part)
{
    if (part->num_free != (pool->frames_per_part * FRAME_SIZE - sizeof(mempool_small_part) - pool->part_first_offset) / pool->obj_size)
        crash("Attempt to free a pool part which is not empty!");
    
    if (prev_part != NULL) prev_part->next_part = part->next_part;
    else pool->parts_empty = part->next_part;
    
    pool->num_total -= part->num_free;
    pool->num_free -= part->num_free;
    
    kmem_page_global_free(part, pool->frames_per_part);
}

static bool _small_pool_part_is_allocated(mempool_small_part* part, void* obj)
{
    mempool_fixed_free_slot* s;
    
    for (s = part->first_free; s != NULL && s != obj; s = s->next) ;
    
    return s == NULL;
}

void kmem_pool_small_init(mempool_small* pool, const char* name, uint32 obj_size, uint32 obj_align)
{
    // When slots are free, a mempool_fixed_free_slot will be stored in place of
    // an actual object.
    if (obj_size < sizeof(mempool_fixed_free_slot))
        obj_size = sizeof(mempool_fixed_free_slot);
    
    // Check that the size of each object aligns the next object
    obj_size += -(obj_size & (obj_align - 1));
    
    spinlock_init(&pool->lock);
    pool->name = name;
    
    pool->obj_size = obj_size;
    
    pool->frames_per_part = 1; // TODO Reconsider this...
    pool->part_first_offset = -(sizeof(mempool_small_part) & (obj_align - 1));
    
    pool->num_total = pool->num_free = 0;
    
    pool->parts_empty = pool->parts_partial = pool->parts_full = NULL;
    
    spinlock_acquire(&small_pool_list_lock);
    pool->next = small_pool_list;
    small_pool_list = pool;
    spinlock_release(&small_pool_list_lock);
}

void* kmem_pool_small_alloc(mempool_small* pool, frame_alloc_flags flags)
{
    mempool_small_part* p;
    mempool_fixed_free_slot* s;
    
    spinlock_acquire(&pool->lock);
    if (pool->parts_partial != NULL)
    {
        p = pool->parts_partial;
        
        p->num_free--;
        pool->num_free--;
        
        s = p->first_free;
        p->first_free = s->next;
        
        if (p->num_free == 0)
        {
            pool->parts_partial = p->next_part;
            p->next_part = pool->parts_full;
            pool->parts_full = p;
        }
    }
    else
    {
        if (pool->parts_empty == NULL)
            _small_pool_part_alloc(pool, flags);
        
        p = pool->parts_empty;
        
        if (p != NULL)
        {
            p->num_free--;
            pool->num_free--;
            
            s = p->first_free;
            p->first_free = s->next;
            
            pool->parts_empty = p->next_part;
            p->next_part = pool->parts_partial;
            pool->parts_partial = p;
        }
        else
        {
            s = NULL;
        }
    }
    
    spinlock_release(&pool->lock);
    
    return (void*)s;
}

void kmem_pool_small_free(mempool_small* pool, void* obj)
{
    addr_v obj_addr = (addr_v)obj;
    mempool_small_part* p;
    mempool_small_part* pp;
    mempool_fixed_free_slot* s;
    
    spinlock_acquire(&pool->lock);
    
    for (p = pool->parts_partial, pp = NULL; p != NULL && (obj_addr < (addr_v)p || obj_addr >= (addr_v)p + pool->frames_per_part * FRAME_SIZE); pp = p, p = p->next_part) ;
    
    if (p == NULL)
    {
        for (p = pool->parts_full, pp = NULL; p != NULL && (obj_addr < (addr_v)p || obj_addr >= (addr_v)p + pool->frames_per_part * FRAME_SIZE); pp = p, p = p->next_part) ;
    }
    
    if (p == NULL)
        crash("Attempt to free an object from a pool it isn't in!");
    else if ((obj_addr - (addr_v)p - sizeof(mempool_small_part) - pool->part_first_offset) % pool->obj_size != 0)
        crash("Attempt to free misaligned object from a pool!");
    else if (!_small_pool_part_is_allocated(p, obj))
        crash("Attempt to free an already freed object from a pool!");
    
    s = (mempool_fixed_free_slot*)obj;
    s->next = p->first_free;
    p->first_free = s;
    
    p->num_free++;
    pool->num_free++;
    
    if (p->num_free == 1)
    {
        if (pp == NULL) pool->parts_full = p->next_part;
        else pp->next_part = p->next_part;
        
        p->next_part = pool->parts_partial;
        pool->parts_partial = p;
    }
    else if (p->num_free == (pool->frames_per_part * FRAME_SIZE - sizeof(mempool_small_part) - pool->part_first_offset) / pool->obj_size)
    {
        if (pp == NULL) pool->parts_partial = p->next_part;
        else pp->next_part = p->next_part;
        
        p->next_part = pool->parts_empty;
        pool->parts_empty = p;
    }
    
    spinlock_release(&pool->lock);
}

void kmem_pool_small_compact(mempool_small* pool)
{
    spinlock_acquire(&pool->lock);
    
    while (pool->parts_empty != NULL)
        _small_pool_part_free(pool, pool->parts_empty, NULL);
    
    spinlock_release(&pool->lock);
}

void kmem_pools_compact()
{
    mempool_small* pool;
    
    spinlock_acquire(&small_pool_list_lock);
    
    for (pool = small_pool_list; pool != NULL; pool = pool->next)
        kmem_pool_small_compact(pool);
    
    spinlock_release(&small_pool_list_lock);
}
