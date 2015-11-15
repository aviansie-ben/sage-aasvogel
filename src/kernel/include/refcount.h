#ifndef REFCOUNT_H
#define REFCOUNT_H

#include <typedef.h>
#include <lock/spinlock.h>
#include <core/crash.h>

struct refcounter;

typedef void (*refcount_destroy)(struct refcounter* refcount);

typedef struct refcounter
{
    uint32 refcount;
    refcount_destroy destroy;
} refcounter;

#define refcount_inc_unsafe(r) __atomic_fetch_add(&(r)->refcount, 1, __ATOMIC_SEQ_CST)
#define refcount_inc(r) \
    ({ \
        uint32 __refcount = refcount_inc_unsafe(r); \
        if (__refcount == 0) \
            crash("Reference count race condition detected!"); \
        __refcount; \
    })

#define refcount_dec(r) \
    ({ \
        uint32 __refcount = __atomic_sub_fetch(&(r)->refcount, 1, __ATOMIC_SEQ_CST); \
        if (__refcount == 0) \
            (r)->destroy(r); \
        else if (__refcount == (uint32)-1) \
            crash("Reference count race condition detected!"); \
        __refcount; \
    })

#define refcount_safe_ptr(type) struct { spinlock lock; type* value; }

#define refcount_safe_ptr_init(sp, val) \
    do \
    { \
        spinlock_init(&(sp)->lock); \
        (sp)->value = (val); \
    } while (0)
#define refcount_safe_ptr_move(sp, val) \
    do \
    { \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        spinlock_acquire(&(sp)->lock); \
        if ((sp)->value != NULL) \
            refcount_dec(&(sp)->value->refcount); \
        (sp)->value = (val); \
        _Pragma("GCC diagnostic pop") \
        spinlock_release(&(sp)->lock); \
    } while (0)
#define refcount_safe_ptr_copy(sp) \
    ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        spinlock_acquire(&(sp)->lock); \
        if ((sp)->value != NULL) \
            refcount_inc(&(sp)->value->refcount); \
        spinlock_release(&(sp)->lock); \
        _Pragma("GCC diagnostic pop") \
        (sp)->value; \
    })
#define refcount_ptr_copy(p) \
    ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        if ((p) != NULL) \
            refcount_inc(&(p)->refcount); \
        _Pragma("GCC diagnostic pop") \
        p; \
    })

// Please forgive me for this...
#define with_refcounted(name, p) \
    for (struct { bool out_done; bool in_done; typeof(p) pointer; } PASTE(__wr_, __LINE__) = { false, false, p }; ; PASTE(__wr_, __LINE__).out_done = true) \
        if (PASTE(__wr_, __LINE__).out_done) \
        { \
            if (PASTE(__wr_, __LINE__).pointer != NULL) \
                refcount_dec(&PASTE(__wr_, __LINE__).pointer->refcount); \
            break; \
        } \
        else \
            for (name = PASTE(__wr_, __LINE__).pointer; !PASTE(__wr_, __LINE__).in_done; PASTE(__wr_, __LINE__).in_done = true)

#endif
