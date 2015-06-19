.intel_syntax noprefix

.globl sched_idle
.type sched_idle, @function
sched_idle:
    hlt
    jmp sched_idle
.size sched_idle, .-sched_idle
