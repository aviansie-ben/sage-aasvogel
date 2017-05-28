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

#define refcount_init(r, d) \
    do \
    { \
        refcounter* __refcounter = r; \
        __refcounter->refcount = 1; \
        __refcounter->destroy = d; \
    } while(0)

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
        refcounter* __refcounter = r; \
        uint32 __refcount = __atomic_sub_fetch(&__refcounter->refcount, 1, __ATOMIC_SEQ_CST); \
        if (__refcount == 0 && __refcounter->destroy != NULL) \
            __refcounter->destroy(__refcounter); \
        else if (__refcount == (uint32)-1) \
            crash("Reference count race condition detected!"); \
        __refcount; \
    })

#define refcount_safe_ptr(type) struct { spinlock lock; type* value; }

#define refcount_safe_ptr_init(sp, val) \
    do \
    { \
        typeof(sp) __safe_ptr = sp; \
        spinlock_init(&__safe_ptr->lock); \
        __safe_ptr->value = (val); \
    } while (0)
#define refcount_safe_ptr_move(sp, val) \
    do \
    { \
        typeof(sp) __safe_ptr = sp; \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        spinlock_acquire(&__safe_ptr->lock); \
        if (__safe_ptr->value != NULL) \
            refcount_dec(&__safe_ptr->value->refcount); \
        __safe_ptr->value = (val); \
        spinlock_release(&__safe_ptr->lock); \
        _Pragma("GCC diagnostic pop") \
    } while (0)
#define refcount_safe_ptr_copy(sp) \
    ({ \
        typeof(sp) __safe_ptr = sp; \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        spinlock_acquire(&__safe_ptr->lock); \
        if (__safe_ptr->value != NULL) \
            refcount_inc(&__safe_ptr->value->refcount); \
        spinlock_release(&__safe_ptr->lock); \
        _Pragma("GCC diagnostic pop") \
        __safe_ptr->value; \
    })
#define refcount_ptr_copy(p) \
    ({ \
        typeof(p) __ptr = p; \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        if (__ptr != NULL) \
            refcount_inc(&__ptr->refcount); \
        _Pragma("GCC diagnostic pop") \
        __ptr; \
    })
#define refcount_ptr_move(p, val) \
    do \
    { \
        typeof(p) __ptr = p; \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Waddress\"") \
        if (__ptr != NULL) \
            refcount_dec(&__ptr->refcount); \
        __ptr = val; \
    } while (0)

#define __UNIQUE_VARIABLE(prefix) PASTE(__, PASTE(prefix, PASTE(_, __LINE__)))

static inline void __rc_cleanup(refcounter** r)
{
    if (*r != NULL)
        refcount_dec(*r);
}

#define with_refcounted(name, p) \
    for (bool __UNIQUE_VARIABLE(done) = false; !__UNIQUE_VARIABLE(done);) \
        for (typeof(p) __UNIQUE_VARIABLE(ptr) = (p); !__UNIQUE_VARIABLE(done);) \
            for (__attribute__((cleanup(__rc_cleanup))) refcounter* __UNIQUE_VARIABLE(rc) = __UNIQUE_VARIABLE(ptr) == NULL ? NULL : &__UNIQUE_VARIABLE(ptr)->refcount; !__UNIQUE_VARIABLE(done);) \
                for (name = __UNIQUE_VARIABLE(ptr); !__UNIQUE_VARIABLE(done); __UNIQUE_VARIABLE(done) = true)
#endif
