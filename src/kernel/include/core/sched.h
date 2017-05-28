#ifndef CORE_SCHED_H
#define CORE_SCHED_H

#include <typedef.h>
#include <core/bootparam.h>
#include <memory/page.h>

#define STS_RUNNING 0
#define STS_READY 1
#define STS_BLOCKING 2
#define STS_SLEEPING 3
#define STS_DEAD 0xdeaddead

#define PIT_IRQ 0
#define CONTEXT_SWITCH_INTERRUPT 0x90

#define TICKS_PER_SECOND 250
#define TICKS_BEFORE_PREEMPT 10
#define PIT_TICK_DIVISOR (1193182 / TICKS_PER_SECOND)
#define MILLISECONDS_PER_TICK (1000 / TICKS_PER_SECOND)

struct sched_process;
struct sched_thread;
struct mutex;

typedef struct
{
    spinlock lock;

    struct sched_thread* first;
    struct sched_thread* last;
} sched_thread_queue;

typedef struct
{
    spinlock lock;

    struct sched_process* first;
    struct sched_process* last;
} sched_process_queue;

typedef struct sched_process
{
    spinlock lock;
    uint64 pid;
    char name[32];

    page_context* address_space;

    uint64 next_tid;
    struct sched_thread* first_thread;
    sched_thread_queue thread_run_queue;

    sched_process_queue* in_queue;
    struct sched_process* next_in_queue;

    struct sched_process* next;
} sched_process;

typedef struct sched_thread
{
    struct sched_process* process;
    uint64 tid;

    uint32 status;
    unsigned long long sleep_until;

    spinlock registers_lock;
    volatile bool registers_dirty;
    regs32_saved_t registers;

    void* stack_low;
    void* stack_high;

    struct sched_thread* next_in_process;

    sched_thread_queue* in_queue;
    struct sched_thread* next_in_queue;

    struct mutex* held_mutexes;

#ifdef SCHED_DEBUG
    unsigned long long creation;
    unsigned long long run_ticks;
#endif
} sched_thread;

typedef void (*sched_thread_function)(void* arg);

extern unsigned long long ticks;
extern sched_process_queue process_run_queue;

extern spinlock process_list_spinlock;
extern sched_process* first_process;

extern sched_process* kernel_process;

/**
 * Initializes the CPU scheduler. This function should only be called once,
 * during kernel initialization.
 */
extern void sched_init(const boot_param* param);

/*
 * IMPORTANT: sched_process_current and sched_thread_current are marked as
 * constant functions, even though they actually aren't. However, they always
 * return the same value when executed from the same context, so they are
 * usually constant. However, these functions should NOT be called from any code
 * which calls sched_switch_thread or sched_switch_any, as those are special
 * functions that may switch the current process/thread without suspending
 * current execution; in these cases, call __sched_process_current and
 * __sched_thread_current instead.
 */

extern sched_process* sched_process_current(void) __const;
extern sched_thread* sched_thread_current(void) __const;

extern sched_process* __sched_process_current(void);
extern sched_thread* __sched_thread_current(void);

extern sched_process* sched_find_process(uint64 pid) __pure;
extern sched_thread* sched_find_thread(sched_process* process, uint64 tid) __pure;

extern int sched_process_create(const char* name, sched_process** process) __warn_unused_result;
extern void sched_process_destroy(sched_process* process);

extern int sched_thread_create(sched_process* process, sched_thread_function func, void* arg, sched_thread** thread) __warn_unused_result;
extern void sched_thread_destroy(sched_thread* thread);

extern void sched_thread_queue_init(sched_thread_queue* queue);
extern void sched_process_queue_init(sched_process_queue* queue);

extern void sched_thread_wake(sched_thread* thread);

extern void sched_thread_enqueue(sched_thread_queue* queue, sched_thread* thread);
extern sched_thread* sched_thread_dequeue(sched_thread_queue* queue);

extern void sched_process_enqueue(sched_process_queue* queue, sched_process* process);
extern sched_process* sched_process_dequeue(sched_process_queue* queue);
extern void sched_process_force_dequeue(sched_process* process);

extern void sched_switch_thread(sched_thread* thread, regs32_t* r);
extern void sched_switch_any(regs32_t* r);

extern void sched_yield(void);
extern void sched_sleep(uint64 milliseconds);
extern void sched_thread_end(void) __attribute__((noreturn));

#endif
