#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <common/rbtree.h>

extern bool panic_flag;

static SpinLock sched_lock;

extern void swtch(KernelContext* new_ctx, KernelContext** old_ctx);

// 1. 初始化调度器资源
// 2. 初始化每个CPU的调度信息
// TODO: 初始化调度器
void init_sched() {
    init_spinlock(&sched_lock);
}

// TODO: 返回当前CPU执行的进程
Proc* thisproc() {

    return 0;
}

// TODO: 为每个新进程 初始化自定义的schinfo
void init_schinfo(struct schinfo* p) {}

// 获取调度锁
void acquire_sched_lock() {
    acquire_spinlock(&sched_lock);
}

// 释放调度锁
void release_sched_lock() {
    release_spinlock(&sched_lock);
}

// 判断进程p是否为ZOOMBIE状态
bool is_zombie(Proc* p) {
    bool r;

    acquire_sched_lock();
    r = p->state == ZOMBIE;
    release_sched_lock();

    return r;
}

// TODO:
// 如果进程状态是 RUNNING/RUNNABLE: 什么都不做
// 如果进程状态是 SLEEPING/UNUSED:  将进程状态设置为 RUNNABLE 并将其添加到调度队列
// 其他情况 : panic
bool activate_proc(Proc* p) {
    return 0;
}

// TODO:
// 更新当前进程的状态为new_state
// 如果new_state=SLEEPING/ZOMBIE, 则从调度队列中移除
static void update_this_state(enum procstate new_state) {}

// TODO: 选择调度队列的下一个进程运行
// 如果没有可运行的进程则返回idle
static Proc* pick_next() {
    return 0;
}

// TODO: you should implement this routinue
// update thisproc to the choosen process
// 将进程p更新为选择的进程
static void update_this_proc(Proc* p) {}

// 接受调度 并将当前进程状态更新为new_state (需持有sched_lock)
void sched(enum procstate new_state) {
    // 获取当前执行的进程
    Proc* this = thisproc();

    // 确保当前进程是 RUNNING 状态
    ASSERT(this->state == RUNNING);

    // 更新当前进程状态为 new_state
    update_this_state(new_state);

    // 选择下一个进程
    Proc* next = pick_next();

    // 更新下一个进程的状态
    update_this_proc(next);

    // 确保下一个进程是 RUNNABLE 状态
    ASSERT(next->state == RUNNABLE);

    // 切换到下一个进程
    next->state = RUNNING;

    // 如果下一个进程不是当前进程, 则切换上下文
    if (next != this)
        swtch(next->kcontext, &this->kcontext);

    // 释放调度锁
    release_sched_lock();
}

u64 proc_entry(void (*entry)(u64), u64 arg) {
    // 释放调度锁
    release_sched_lock();

    // 设置返回地址为entry
    set_return_addr(entry);

    return arg;
}
