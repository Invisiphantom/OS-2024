#pragma once

#include <kernel/proc.h>
#include <common/rbtree.h>

#define NCPU 4

struct sched {
    Proc* proc;      // 当前CPU上运行的进程, 或者为空
    Proc* idle_proc; // 当前CPU专属idle进程
};

struct cpu {
    bool online;           // 是否启用
    struct rb_root_ timer; // 计时器 (红黑树)
    struct sched sched;    // 每个CPU的调度信息
};

extern struct cpu cpus[NCPU];
#define thiscpu (&cpus[cpuid()])

void set_cpu_on();
void set_cpu_off();
