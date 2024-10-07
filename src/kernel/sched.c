#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <common/rbtree.h>

extern bool panic_flag;
extern SpinLock proc_lock;

static SpinLock sched_lock; // 调度锁
static Queue sched_queue;   // 调度队列

extern void swtch(KernelContext** old_ctx, KernelContext* new_ctx);

// TODO: 初始化调度器
void init_sched()
{
    init_spinlock(&sched_lock); // 初始化调度锁
    queue_init(&sched_queue);   // 初始化调度队列
}

// TODO: 返回当前CPU上执行的进程
Proc* thisproc() { return thiscpu->sched.proc; }

// TODO: 为每个新进程 初始化自定义的schinfo
void init_schinfo(struct schinfo* p) { }

// 调用schd()之前, 需要获取sched_lock
void acquire_sched_lock() { acquire_spinlock(&sched_lock); }
void release_sched_lock() { release_spinlock(&sched_lock); }

// 判断进程p是否为ZOOMBIE状态
bool is_zombie(Proc* p)
{
    acquire_sched_lock();
    bool r = (p->state == ZOMBIE);
    release_sched_lock();
    return r;
}

// TODO: 唤醒进程
// 如果进程状态是 RUNNING/RUNNABLE: 什么都不做
// 如果进程状态是 SLEEPING/UNUSED:  将进程状态设置为 RUNNABLE
// 并将其添加到调度队列 其他情况 : panic
bool activate_proc(Proc* p)
{
    if (p->state == RUNNING || p->state == RUNNABLE)
        return true;

    if (p->state == SLEEPING || p->state == UNUSED) {
        // 更新进程状态设置为 RUNNABLE
        acquire_spinlock(&proc_lock);
        p->state = RUNNABLE;
        release_spinlock(&proc_lock);

        // 将其添加到调度队列
        queue_push_lock(&sched_queue, &p->schinfo.sched_node);

        return true;
    }

    printk("activate_proc: invalid state %d\n", p->state);
    PANIC();
    return false;
}

// TODO: 更新当前进程的状态为new_state
static void update_this_state(enum procstate new_state)
{
    acquire_spinlock(&proc_lock);

    // 更新状态为new_state
    auto p = thisproc();
    p->state = new_state;

    // 如果new_state=SLEEPING/ZOMBIE, 则从调度队列中移除
    if (new_state == SLEEPING || new_state == ZOMBIE)
        queue_detach_lock(&sched_queue, &p->schinfo.sched_node);

    release_spinlock(&proc_lock);
}

// TODO: 从调度队列中挑选进程
// 如果没有可运行的进程则返回idle
static Proc* pick_next()
{
    // 如果没有可运行进程, 则返回idle
    if (queue_empty(&sched_queue))
        return thiscpu->sched.idle_proc;

    // 选择队列头的进程
    auto node = queue_front(&sched_queue);
    auto p = container_of(node, Proc, schinfo.sched_node);

    // 将该进程从队列头 移动到 队列尾
    queue_lock(&sched_queue);
    queue_pop(&sched_queue);
    queue_push(&sched_queue, node);
    queue_unlock(&sched_queue);

    return p;
}

// TODO: 将进程p更新为CPU选择的进程
static void update_this_proc(Proc* p)
{
    acquire_spinlock(&proc_lock);

    auto c = thiscpu;
    c->sched.proc = p;
    p->state = RUNNABLE;

    release_spinlock(&proc_lock);
}

// 接受调度 并将当前进程状态更新为new_state (需持有sched_lock)
void sched(enum procstate new_state)
{
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

    // 由进程负责释放锁 并在返回到调度器之前重新获取锁
    // 将旧上下文压栈 并用this->kcontext保存sp
    // 然后从next->kcontext的sp加载新上下文
    if (next != this)
        swtch(&this->kcontext, next->kcontext);

    // 释放调度锁
    release_sched_lock();
}

u64 proc_entry(void (*entry)(u64), u64 arg)
{
    // 释放调度锁
    release_sched_lock();

    // 设置返回地址为entry
    set_return_addr(entry);

    return arg;
}
