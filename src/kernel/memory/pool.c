#include <memory/pool.h>
#include <memory/phys.h>
#include <memory/virt.h>

#include <core/crash.h>
#include <assert.h>

#define MAP_TYPE_NONE 0
#define MAP_TYPE_SMALL 1
#define MAP_TYPE_FRAMES 2

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

typedef struct
{
    uint32 type;
    
    union
    {
        mempool_small* pool_small;
        struct
        {
            uint32 num_frames;
            addr_v first_frame_address;
        };
    };
} mempool_map;

typedef struct
{
    mempool_map mappings[FRAME_SIZE / sizeof(mempool_map)];
} mempool_map_table;

frame_alloc_flags mapping_frame_flags = FA_EMERG;
mempool_map_table* mapping_tables[(0x40000 * sizeof(mempool_map)) / sizeof(mempool_map_table)];

static mempool_small pool_gen_16;
static mempool_small pool_gen_32;
static mempool_small pool_gen_64;
static mempool_small pool_gen_128;
static mempool_small pool_gen_256;

#ifdef MEMPOOL_DEBUG
static void _fill_deadbeef(uint8* o, size_t size)
{
    for (uint8* f = o; (size_t)(f - o) < size; f++)
    {
        switch ((f - o) % 4)
        {
            case 0:
                *f = 0xef;
                break;
            case 1:
                *f = 0xbe;
                break;
            case 2:
                *f = 0xad;
                break;
            case 3:
                *f = 0xde;
                break;
        }
    }
}
#else
#define _fill_deadbeef(o, s) ((void)(o), (void)(s))
#endif

static mempool_map* _get_mapping(addr_v address, bool create)
{
    uint32 table = ((address - 0xc0000000) / FRAME_SIZE) / (FRAME_SIZE / sizeof(mempool_map));
    uint32 map = ((address - 0xc0000000) / FRAME_SIZE) % (FRAME_SIZE / sizeof(mempool_map));
    
    if (mapping_tables[table] == NULL && create)
        mapping_tables[table] = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, mapping_frame_flags, 1);
    
    return mapping_tables[table] != NULL ? &mapping_tables[table]->mappings[map] : NULL;
}

static void _small_pool_part_alloc(mempool_small* pool, frame_alloc_flags flags)
{
    mempool_small_part* p;
    mempool_fixed_free_slot* s;
    
    size_t i;
    size_t j;
    
    p = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, flags, pool->frames_per_part);
    
    if (p == NULL)
        return;
    
    for (i = 0; i < pool->frames_per_part; i++)
    {
        mempool_map* map = _get_mapping((addr_v)p + (i * FRAME_SIZE), true);
        
        if (map == NULL)
        {
            for (j = 0; j < i; j++)
            {
                map = _get_mapping((addr_v)p + j * FRAME_SIZE, false);
                map->type = MAP_TYPE_NONE;
            }
            
            kmem_page_global_free(p, pool->frames_per_part);
            return;
        }
        
        map->type = MAP_TYPE_SMALL;
        map->pool_small = pool;
    }
    
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
    
    for (size_t i = 0; i < pool->frames_per_part; i++)
    {
        mempool_map* map = _get_mapping((addr_v)part + (i * FRAME_SIZE), false);
        assert(map != NULL && map->type == MAP_TYPE_SMALL && map->pool_small == pool);
        
        map->type = MAP_TYPE_NONE;
    }
    
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

void kmem_pool_small_init(mempool_small* pool, const char* name, uint32 obj_size, uint32 obj_align, frame_alloc_flags frame_flags)
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
    pool->frame_flags = frame_flags;
    
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
            _small_pool_part_alloc(pool, flags | pool->frame_flags);
        
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
    
    _fill_deadbeef((uint8*)s, pool->obj_size);
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
    
    _fill_deadbeef((uint8*)obj, pool->obj_size);
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

void kmem_pool_generic_init(void)
{
    kmem_pool_small_init(&pool_gen_16, "generic_16", 16, 4, 0);
    kmem_pool_small_init(&pool_gen_32, "generic_32", 32, 4, 0);
    kmem_pool_small_init(&pool_gen_64, "generic_64", 64, 4, 0);
    kmem_pool_small_init(&pool_gen_128, "generic_128", 128, 4, 0);
    kmem_pool_small_init(&pool_gen_256, "generic_256", 256, 4, 0);
}

void* kmem_pool_generic_alloc(size_t size, frame_alloc_flags flags)
{
    if (size <= 16)
        return kmem_pool_small_alloc(&pool_gen_16, flags);
    else if (size <= 32)
        return kmem_pool_small_alloc(&pool_gen_32, flags);
    else if (size <= 64)
        return kmem_pool_small_alloc(&pool_gen_64, flags);
    else if (size <= 128)
        return kmem_pool_small_alloc(&pool_gen_128, flags);
    else if (size <= 256)
        return kmem_pool_small_alloc(&pool_gen_256, flags);
    else
    {
        // TODO Use space more efficiently!
        
        uint32 num_pages = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        addr_v address = (addr_v) kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, flags, num_pages);
        
        if (address == 0) return NULL;
        
        for (uint32 i = 0; i < num_pages; i++)
        {
            mempool_map* map = _get_mapping(address + i * FRAME_SIZE, true);
            
            if (map == NULL)
            {
                for (uint32 j = 0; j < num_pages; j++)
                {
                    map = _get_mapping(address + j * FRAME_SIZE, false);
                    map->type = MAP_TYPE_NONE;
                }
                
                kmem_page_global_free((void*) address, num_pages);
                return NULL;
            }
            
            map->type = MAP_TYPE_FRAMES;
            map->num_frames = num_pages;
            map->first_frame_address = address;
        }
        
        _fill_deadbeef((uint8*) address, num_pages * FRAME_SIZE);
        return (void*) address;
    }
}

void kmem_pool_generic_free(void* obj)
{
    assert((uint32)obj >= 0xc0000000);
    
    mempool_map* map = _get_mapping((addr_v) obj, false);
    
    if (map == NULL || map->type == MAP_TYPE_NONE)
        crash("Attempt to free an object which isn't in memory allocated to a pool!");
    
    if (map->type == MAP_TYPE_SMALL)
    {
        kmem_pool_small_free(map->pool_small, obj);
    }
    else if (map->type == MAP_TYPE_FRAMES)
    {
        addr_v address = map->first_frame_address;
        uint32 num_frames = map->num_frames;
        
        for (uint32 i = 0; i < num_frames; i++)
        {
            map = _get_mapping(address + i * FRAME_SIZE, false);
            assert(map != NULL && map->type == MAP_TYPE_FRAMES && map->num_frames == num_frames && map->first_frame_address == address);
            
            map->type = MAP_TYPE_NONE;
        }
        
        kmem_page_global_free((void*) address, num_frames);
    }
    else
    {
        crash("Unknown mapping type!");
    }
}

void kmem_pool_generic_compact(void)
{
    kmem_pool_small_compact(&pool_gen_16);
    kmem_pool_small_compact(&pool_gen_32);
    kmem_pool_small_compact(&pool_gen_64);
    kmem_pool_small_compact(&pool_gen_128);
    kmem_pool_small_compact(&pool_gen_256);
}

void kmem_pools_compact(void)
{
    mempool_small* pool;
    
    spinlock_acquire(&small_pool_list_lock);
    
    for (pool = small_pool_list; pool != NULL; pool = pool->next)
        kmem_pool_small_compact(pool);
    
    spinlock_release(&small_pool_list_lock);
}
