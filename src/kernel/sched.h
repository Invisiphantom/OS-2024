#pragma once

#include <kernel/proc.h>

void init_sched();
void init_schinfo(struct schinfo*);

Proc* thisproc();
bool activate_proc(Proc*);
void acquire_sched();
void release_sched();
void sched(enum procstate new_state);
u64 proc_entry(void (*entry)(u64), u64 arg);

// 获取调度锁 并开始调度
#define yield() (acquire_sched(), sched(RUNNABLE))

