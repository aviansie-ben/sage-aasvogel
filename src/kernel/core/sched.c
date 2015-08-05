#include <core/sched.h>
#include <memory/pool.h>
#include <string.h>
#include <assert.h>
#include <core/gdt.h>
#include <core/idt.h>
#include <hwio.h>

#include <core/klog.h>

#define THREAD_STACK_SIZE 0x40000
#define THREAD_EFLAGS ((1 << 1) | (1 << 9))

#define PIT_CHANNEL_0_DATA 0x40
#define PIT_COMMAND 0x43
#define PIT_MODE_COMMAND 0x36 // Select channel 0 with lobyte/hibyte access mode and operating
                              // mode 3 (square wave generator)

// TODO: Deal with muliple processors
static sched_process* current_process;
static sched_thread* current_thread;

static uint64 next_pid = 0;

#ifndef SCHED_NO_PREEMPT
static unsigned long long ticks_until_preempt = TICKS_BEFORE_PREEMPT;
#endif

unsigned long long ticks = 0;
sched_process_queue process_run_queue;

static spinlock first_process_spinlock;
sched_process* first_process = NULL;

static sched_thread* idle_thread;

static sched_thread_queue sleep_queue;

static mempool_small process_pool;
static mempool_small thread_pool;

void sched_idle(void);

static void init_registers(regs32_saved_t* r, uint32 stack, uint32 entry)
{
    r->gs = r->fs = r->es = r->ds = r->ss = GDT_KERNEL_DATA;
    r->cs = GDT_KERNEL_CODE;
    
    r->edi = r->esi = r->ebp = r->ebx = r->edx = r->ecx = r->eax = 0;
    r->eflags = THREAD_EFLAGS;
    
    r->eip = entry;
    r->esp = stack;
}

static sched_thread* alloc_init_thread(sched_process* p)
{
    sched_thread* t = kmem_pool_small_alloc(&thread_pool, 0);
    
    if (t == NULL)
        return NULL;
    
    t->process = p;
    t->tid = (p == NULL) ? 0 : p->next_tid++;
    
    t->status = STS_READY;
    
    t->stack_low = t->stack_high = NULL;
    t->in_queue = NULL;
    
    t->next_in_process = (p == NULL) ? NULL : p->first_thread;
    
    if (p != NULL)
    {
        p->first_thread = t;
        
        spinlock_acquire(&p->thread_run_queue.lock);
        sched_thread_enqueue(&p->thread_run_queue, t);
        spinlock_release(&p->thread_run_queue.lock);
    }
    
#ifdef SCHED_DEBUG
    t->creation = ticks;
    t->run_ticks = 0;
#endif
    
    if (p != NULL)
        klog(KLOG_LEVEL_DEBUG, "Created thread %ld under process %ld (%s)\n", t->tid, p->pid, p->name);
    else
        klog(KLOG_LEVEL_DEBUG, "Created disconnected thread\n");
    
    return t;
}

static sched_process* alloc_init_process(const char* name)
{
    sched_process* p = kmem_pool_small_alloc(&process_pool, 0);
    
    if (p == NULL)
        return NULL;
    
    spinlock_init(&p->lock);
    
    p->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_RELAXED);
    
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    
    p->next_tid = 0;
    p->first_thread = NULL;
    sched_thread_queue_init(&p->thread_run_queue);
    
    p->next = NULL;
    
    spinlock_acquire(&p->lock);
    
    spinlock_acquire(&first_process_spinlock);
    p->next = first_process;
    first_process = p;
    spinlock_release(&first_process_spinlock);
    
    spinlock_acquire(&process_run_queue.lock);
    sched_process_enqueue(&process_run_queue, p);
    spinlock_release(&process_run_queue.lock);
    
    spinlock_release(&p->lock);
    
    klog(KLOG_LEVEL_DEBUG, "Created process %ld (%s)\n", p->pid, p->name);
    return p;
}

static void pit_init(uint32 divisor)
{
    outb(PIT_COMMAND, PIT_MODE_COMMAND);
    outb(PIT_CHANNEL_0_DATA, divisor & 0xff);
    outb(PIT_CHANNEL_0_DATA, (divisor >> 8) & 0xff);
}

static void pit_tick_handle(regs32_t* r)
{
    ticks++;
    
#ifdef SCHED_DEBUG
    if (current_thread != NULL)
    {
        current_thread->run_ticks++;
    }
#endif
    
    spinlock_acquire(&sleep_queue.lock);
    while (sleep_queue.first != NULL && sleep_queue.first->sleep_until <= ticks)
    {
        sched_thread* t = sched_thread_dequeue(&sleep_queue);
        
        spinlock_release(&sleep_queue.lock);
        spinlock_acquire(&t->process->thread_run_queue.lock);
        
        sched_thread_enqueue(&t->process->thread_run_queue, t);
        t->status = STS_READY;
        
        spinlock_release(&t->process->thread_run_queue.lock);
        spinlock_acquire(&sleep_queue.lock);
    }
    spinlock_release(&sleep_queue.lock);
    
#ifndef SCHED_NO_PREEMPT
    ticks_until_preempt--;
    if (ticks_until_preempt == 0)
#else
    if (current_thread == NULL)
#endif
    {
        if (current_thread != NULL && current_thread->status != STS_DEAD)
        {
            spinlock_acquire(&current_thread->process->thread_run_queue.lock);
            sched_thread_enqueue(&current_thread->process->thread_run_queue, current_thread);
            current_thread->status = STS_READY;
            spinlock_release(&current_thread->process->thread_run_queue.lock);
        }
        
        sched_switch_any(r);
    }
}

static void yield_interrupt_handle(regs32_t* r)
{
    sched_switch_any(r);
}

void sched_init(const boot_param* param)
{
    spinlock_init(&first_process_spinlock);
    
    kmem_pool_small_init(&process_pool, "sched_process pool", sizeof(sched_process), __alignof__(sched_process));
    kmem_pool_small_init(&thread_pool, "sched_thread pool", sizeof(sched_thread), __alignof__(sched_thread));
    
    sched_process_queue_init(&process_run_queue);
    sched_thread_queue_init(&sleep_queue);
    
    current_process = first_process = alloc_init_process("kernel");
    if (current_process == NULL)
        crash("Failed to initialize kernel process!");
    
    sched_process_enqueue(&process_run_queue, current_process);
    
    current_thread = alloc_init_thread(first_process);
    if (current_thread == NULL)
        crash("Failed to initialize first kernel thread!");
    
    current_thread->registers_dirty = true;
    sched_thread_dequeue(&current_process->thread_run_queue);
    
    idle_thread = alloc_init_thread(NULL);
    if (idle_thread == NULL)
        crash("Failed to initialize idle thread!");
    init_registers(&idle_thread->registers, (uint32)idle_thread->stack_high, (uint32) sched_idle);
    
    // Register the PIT tick handler and enable the PIT
    idt_register_irq_handler(0, pit_tick_handle);
    pit_init(PIT_TICK_DIVISOR);
    idt_set_irq_enabled(0, true);
    
    // The context switch interrupt should NOT be callable from userspace and should disable interrupts
    idt_set_ext_handler_flags(CONTEXT_SWITCH_INTERRUPT - IDT_EXT_START, 0x8E);
    idt_register_ext_handler(CONTEXT_SWITCH_INTERRUPT - IDT_EXT_START, yield_interrupt_handle);
}

sched_process* sched_process_current(void)
{
    return current_process;
}

sched_thread* sched_thread_current(void)
{
    return current_thread;
}

sched_process* __sched_process_current(void)
{
    return current_process;
}

sched_thread* __sched_thread_current(void)
{
    return current_thread;
}

int sched_process_create(const char* name, sched_process** process)
{
    sched_process* p = alloc_init_process(name);
    
    if (p == NULL)
        return -1; // TODO Come up with proper error codes
    
    *process = p;
    return 0;
}

void sched_process_destroy(sched_process* process)
{
    crash("Not implemented!");
}

int sched_thread_create(sched_process* process, sched_thread_function func, void* arg, sched_thread** thread)
{
    void** stack_low = kmem_page_global_alloc(PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, 0, THREAD_STACK_SIZE / FRAME_SIZE);
    void** stack_high = stack_low + THREAD_STACK_SIZE / sizeof(void*);
    
    if (stack_low == NULL)
        return -1; // TODO Come up with proper error codes
    
    klog(KLOG_LEVEL_DEBUG, "Allocated stack from 0x%x to 0x%x (size %d)\n", stack_low, stack_high, THREAD_STACK_SIZE);
    
    *(stack_high - 1) = arg;
    *(stack_high - 2) = NULL;
    
    sched_thread* t = alloc_init_thread(process);
    
    if (t == NULL)
    {
        spinlock_release(&process->lock);
        kmem_page_global_free(stack_low, THREAD_STACK_SIZE / FRAME_SIZE);
        return -1; // TODO Come up with proper error codes
    }
    
    init_registers(&t->registers, (uint32)(stack_high - 2), (uint32)func);
    t->stack_low = stack_low;
    t->stack_high = stack_high;
    
    if (thread != NULL)
        *thread = t;
    
    return 0;
}

void sched_thread_destroy(sched_thread* thread)
{
    if (thread->in_queue != NULL)
        sched_thread_force_dequeue(thread);
    
    kmem_page_global_free(thread->stack_low, THREAD_STACK_SIZE / FRAME_SIZE);
    
    if (thread->process->first_thread == thread)
    {
        thread->process->first_thread = thread->next_in_process;
    }
    else
    {
        sched_thread* prev_thread;
        
        for (prev_thread = thread->process->first_thread; prev_thread != NULL && prev_thread->next_in_process != thread; prev_thread = prev_thread->next_in_process) ;
        
        if (prev_thread != NULL)
            prev_thread->next_in_process = thread->next_in_process;
    }
    
    thread->status = STS_DEAD;
    klog(KLOG_LEVEL_DEBUG, "Destroyed thread %ld under process %ld (%s)\n", thread->tid, thread->process->pid, thread->process->name);
    
    kmem_pool_small_free(&thread_pool, thread);
}

void sched_thread_queue_init(sched_thread_queue* queue)
{
    spinlock_init(&queue->lock);
    queue->first = queue->last = NULL;
}

void sched_process_queue_init(sched_process_queue* queue)
{
    spinlock_init(&queue->lock);
    queue->first = queue->last = NULL;
}

void sched_thread_wake(sched_thread* thread)
{
    assert(thread->in_queue == NULL && thread->status == STS_BLOCKING);
    
    spinlock_acquire(&thread->process->thread_run_queue.lock);
    thread->status = STS_READY;
    sched_thread_enqueue(&thread->process->thread_run_queue, thread);
    spinlock_release(&thread->process->thread_run_queue.lock);
}

void sched_thread_enqueue(sched_thread_queue* queue, sched_thread* thread)
{
    if (queue->first == NULL)
        queue->first = thread;
    else
        queue->last->next_in_queue = thread;
    
    thread->next_in_queue = NULL;
    thread->in_queue = queue;
    queue->last = thread;
}

sched_thread* sched_thread_dequeue(sched_thread_queue* queue)
{
    sched_thread* t = queue->first;
    
    if (t == NULL)
        return NULL;
    
    queue->first = t->next_in_queue;
    t->in_queue = NULL;
    if (queue->last == t)
        queue->last = NULL;
    
    return t;
}

void sched_thread_force_dequeue(sched_thread* thread)
{
    sched_thread* prev_thread;
    sched_thread_queue* queue;
    
    while (true)
    {
        queue = thread->in_queue;
        spinlock_acquire(&queue->lock);
        
        if (thread->in_queue == queue)
        {
            if (queue->first == thread)
            {
                queue->first = thread->next_in_queue;
                
                if (queue->last == thread)
                    queue->last = NULL;
            }
            else
            {
                for (prev_thread = queue->first; prev_thread != NULL && prev_thread->next_in_queue != thread; prev_thread = prev_thread->next_in_queue) ;
                
                if (prev_thread != NULL)
                    prev_thread->next_in_queue = thread->next_in_queue;
                
                if (queue->last == thread)
                    queue->last = prev_thread;
            }
            
            spinlock_release(&queue->lock);
            return;
        }
        else
        {
            spinlock_release(&queue->lock);
        }
    }
}

void sched_process_enqueue(sched_process_queue* queue, sched_process* process)
{
    if (queue->first == NULL)
        queue->first = process;
    else
        queue->last->next_in_queue = process;
    
    process->next_in_queue = NULL;
    process->in_queue = queue;
    queue->last = process;
}

sched_process* sched_process_dequeue(sched_process_queue* queue)
{
    sched_process* p = queue->first;
    
    if (p == NULL)
        return NULL;
    
    queue->first = p->next_in_queue;
    p->in_queue = NULL;
    if (queue->last == p)
        queue->last = NULL;
    
    return p;
}

static void save_registers(const regs32_t* ir, regs32_saved_t* sr)
{
    sr->gs = ir->gs;
    sr->fs = ir->fs;
    sr->es = ir->es;
    sr->ds = ir->ds;
    
    sr->edi = ir->edi;
    sr->esi = ir->esi;
    sr->ebp = ir->ebp;
    sr->ebx = ir->ebx;
    sr->edx = ir->edx;
    sr->ecx = ir->ecx;
    sr->eax = ir->eax;
    
    sr->eip = ir->eip;
    sr->cs = ir->cs;
    sr->eflags = ir->eflags;
    
    sr->esp = ir->esp;
    sr->ss = ir->ss;
}

static void load_registers(regs32_t* ir, const regs32_saved_t* sr)
{
    ir->gs = sr->gs;
    ir->fs = sr->fs;
    ir->es = sr->es;
    ir->ds = sr->ds;
    
    ir->edi = sr->edi;
    ir->esi = sr->esi;
    ir->ebp = sr->ebp;
    ir->ebx = sr->ebx;
    ir->edx = sr->edx;
    ir->ecx = sr->ecx;
    ir->eax = sr->eax;
    
    ir->eip = sr->eip;
    ir->cs = sr->cs;
    ir->eflags = sr->eflags;
    
    ir->esp = sr->esp;
    ir->ss = sr->ss;
}

void sched_switch_thread(sched_thread* thread, regs32_t* r)
{
    if (thread == current_thread)
        return;
    
    assert(thread->status == STS_READY);
    assert(current_thread == NULL || current_thread->registers_dirty || current_thread->status == STS_DEAD);
    assert(thread->stack_low == NULL || (thread->registers.esp <= (uint32)thread->stack_high && thread->registers.esp >= (uint32)thread->stack_low));
    
#ifdef SCHED_SWITCH_DEBUG
    if (current_process == NULL)
    {
        klog(KLOG_LEVEL_DEBUG, "Switching from idle to p%ld (%s), t%ld\n",
            thread->process->pid, thread->process->name, thread->tid);
    }
    else if (current_thread == NULL)
    {
        klog(KLOG_LEVEL_DEBUG, "Switching from p%ld (%s) to p%ld (%s), t%ld\n",
            current_process->pid, current_process->name,
            thread->process->pid, thread->process->name, thread->tid);
    }
    else
    {
        klog(KLOG_LEVEL_DEBUG, "Switching from p%ld (%s), t%ld to p%ld (%s), t%ld\n",
            current_process->pid, current_process->name, current_thread->tid,
            thread->process->pid, thread->process->name, thread->tid);
    }
#endif
    
    if (current_thread != NULL)
    {
        spinlock_acquire(&current_thread->registers_lock);
        save_registers(r, &current_thread->registers);
        spinlock_release(&current_thread->registers_lock);
        current_thread->registers_dirty = false;
    }
    
    current_thread = thread;
    current_process = thread->process;
    
    // TODO Switch address spaces
    
    // Wait until registers are fully saved before attempting to acquire the spinlock
    while (current_thread->registers_dirty)
        asm volatile ("pause");
    
    current_thread->status = STS_RUNNING;
    current_thread->registers_dirty = true;
    
    spinlock_acquire(&current_thread->registers_lock);
    load_registers(r, &current_thread->registers);
    spinlock_release(&current_thread->registers_lock);
}

void sched_switch_any(regs32_t* r)
{
    sched_process* begin_process;
    sched_process* new_process;
    sched_thread* new_thread;
    uint32 old_esp;
    
    spinlock_acquire(&process_run_queue.lock);
    new_process = begin_process = sched_process_dequeue(&process_run_queue);
    
    while (new_process != NULL)
    {
        sched_process_enqueue(&process_run_queue, new_process);
        
        spinlock_acquire(&new_process->thread_run_queue.lock);
        new_thread = sched_thread_dequeue(&new_process->thread_run_queue);
        spinlock_release(&new_process->thread_run_queue.lock);
        
        if (new_thread != NULL)
            break;
        
        new_process = sched_process_dequeue(&process_run_queue);
        
        if (new_process == begin_process)
        {
            sched_process_enqueue(&process_run_queue, new_process);
            new_process = NULL;
            break;
        }
    }
    
    spinlock_release(&process_run_queue.lock);
    
    if (new_process != NULL && new_thread != NULL)
    {
        sched_switch_thread(new_thread, r);
        
#ifndef SCHED_NO_PREEMPT
        ticks_until_preempt = TICKS_BEFORE_PREEMPT;
#endif
    }
    else
    {
#ifdef SCHED_SWITCH_DEBUG
        if (current_process != NULL)
        {
            if (current_thread != NULL)
            {
                klog(KLOG_LEVEL_DEBUG, "Switching from p%ld (%s), t%ld to idle\n",
                    current_process->pid, current_process->name, current_thread->tid);
            }
            else
            {
                klog(KLOG_LEVEL_DEBUG, "Switching from p%ld (%s) to idle\n",
                    current_process->pid, current_process->name);
            }
        }
#endif
        
        if (current_thread != NULL)
        {
            spinlock_acquire(&current_thread->registers_lock);
            save_registers(r, &current_thread->registers);
            spinlock_release(&current_thread->registers_lock);
            current_thread->registers_dirty = false;
        }
        
        current_process = NULL;
        current_thread = NULL;
        
        old_esp = r->esp;
        load_registers(r, &idle_thread->registers);
        r->esp = old_esp;
        
#ifndef SCHED_NO_PREEMPT
        ticks_until_preempt = 1;
#endif
    }
}

void sched_yield(void)
{
    asm volatile ("int %0" : : "i" (CONTEXT_SWITCH_INTERRUPT));
}

void sched_sleep(uint64 milliseconds)
{
    uint64 nticks = milliseconds / MILLISECONDS_PER_TICK;
    uint32 eflags;
    sched_thread* prev_thread;
    
    eflags = eflags_save();
    asm volatile ("cli");
    
    if (nticks == 0)
    {
        spinlock_acquire(&current_process->thread_run_queue.lock);
        sched_thread_enqueue(&current_process->thread_run_queue, current_thread);
        current_thread->status = STS_READY;
        spinlock_release(&current_process->thread_run_queue.lock);
        
        sched_yield();
        eflags_load(eflags);
        
        return;
    }
    
    current_thread->status = STS_SLEEPING;
    current_thread->sleep_until = ticks + nticks;
    
    spinlock_acquire(&sleep_queue.lock);
    current_thread->in_queue = &sleep_queue;
    
    if (sleep_queue.first == NULL || sleep_queue.first->sleep_until >= current_thread->sleep_until)
    {
        current_thread->next_in_queue = sleep_queue.first;
        sleep_queue.first = current_thread;
    }
    else
    {
        for (prev_thread = sleep_queue.first; prev_thread->next_in_queue != NULL && prev_thread->next_in_queue->sleep_until < current_thread->sleep_until; prev_thread = prev_thread->next_in_queue) ;
        
        current_thread->next_in_queue = prev_thread->next_in_queue;
        prev_thread->next_in_queue = current_thread;
    }
    
    spinlock_release(&sleep_queue.lock);
    
    sched_yield();
    eflags_load(eflags);
}
