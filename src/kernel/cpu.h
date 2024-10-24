#pragma once

#include <kernel/proc.h>
#include <common/rbtree.h>

#define NCPU 4

struct sched {
    Proc* proc;      // 当前CPU上运行的进程, 或者为空
    Proc* idle_proc; // 当前CPU专属idle进程
    Proc* before_proc;  // 记录跳转到idle之前的进程
};

struct cpu {
    bool online;           // 是否启用
    struct rb_root_ timer; // 计时器 (红黑树)
    struct sched sched;    // 每个CPU的调度信息
};

extern struct cpu cpus[NCPU];
#define thiscpu (&cpus[cpuid()])

// 定时器
struct timer {
    bool triggered;                 // 是否已经被触发
    int elapse;                     // 下次触发时间
    u64 _key;                       // 红黑树比较中的key
    struct rb_node_ _node;          // 红黑树结点
    void (*handler)(struct timer*); // 处理函数
    u64 data;                       // 定时触发次数
};

void init_clock_handler();

void set_cpu_on();
void set_cpu_off();

void set_cpu_timer(struct timer* timer);
void cancel_cpu_timer(struct timer* timer);