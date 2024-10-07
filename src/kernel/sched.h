#pragma once

#include <kernel/proc.h>



void init_sched();
void init_schinfo(struct schinfo *);

bool activate_proc(Proc *);
bool is_zombie(Proc *);
void acquire_sched_lock();
void release_sched_lock();
void sched(enum procstate new_state);
u64 proc_entry(void (*entry)(u64), u64 arg);

// 获取调度锁 并开始调度
#define yield() (acquire_sched_lock(), sched(RUNNABLE))

Proc *thisproc();
