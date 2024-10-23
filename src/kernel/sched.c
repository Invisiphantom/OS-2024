#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <common/rbtree.h>

extern bool panic_flag;
extern SpinLock proc_lock;

static SpinLock sched_lock; // 调度函数锁
static Queue sched_queue;   // 调度队列

// 调度定时器
static struct timer sched_timer[NCPU];
static void time_sched(struct timer* t) { sched(RUNNABLE); }

extern void swtch(KernelContext** old_ctx, KernelContext* new_ctx);

// 初始化调度器
void init_sched()
{
    init_spinlock(&sched_lock); // 初始化调度锁
    queue_init(&sched_queue);   // 初始化调度队列

    // 初始化调度定时器
    for (int i = 0; i < NCPU; i++) {
        sched_timer[i].elapse = 100; // 间隔时间
        sched_timer[i].handler = time_sched;
    }
}

// 为每个新进程 初始化自定义的schinfo
void init_schinfo(struct schinfo* p) { return; }

// 返回当前CPU上执行的进程
Proc* thisproc() { return thiscpu->sched.proc; }

// 调用schd()之前, 需要获取sched_lock
void acquire_sched_lock() { acquire_spinlock(&sched_lock); }
void release_sched_lock() { release_spinlock(&sched_lock); }

// 唤醒进程
// 如果进程状态是 RUNNING/RUNNABLE: 什么都不做
// 如果进程状态是 SLEEPING/UNUSED:  将进程状态设置为 RUNNABLE 并将其添加到调度队列
// 其他情况: panic
bool activate_proc(Proc* p)
{
    acquire_spinlock(&proc_lock);

    if (p->state == ZOMBIE) {
        release_spinlock(&proc_lock);
        return false;
    }

    if (p->state == RUNNING || p->state == RUNNABLE) {
        release_spinlock(&proc_lock);
        return true;
    }

    if (p->state == SLEEPING || p->state == UNUSED) {
        // 更新进程状态设置为 RUNNABLE
        p->state = RUNNABLE;

        // 将其添加到调度队列
        queue_push_lock(&sched_queue, &p->schinfo.sched_node);

        release_spinlock(&proc_lock);
        return true;
    }

    printk("activate_proc: invalid state %d\n", p->state);
    PANIC();
    return false;
}

// 更新当前进程的状态为new_state
static void update_this_state(enum procstate new_state)
{
    // 更新状态为new_state
    auto p = thisproc();
    p->state = new_state;

    // 如果new_state=SLEEPING/ZOMBIE, 则从调度队列中移除
    if (new_state == SLEEPING || new_state == ZOMBIE)
        queue_detach_lock(&sched_queue, &p->schinfo.sched_node);
}

// 从调度队列中挑选进程
// 如果没有可运行的进程则返回idle
static Proc* pick_next()
{
    // 如果调度队列中没有进程, 则返回idle
    if (queue_empty(&sched_queue))
        return thiscpu->sched.idle_proc;

    queue_lock(&sched_queue); // 获取调度队列锁

    // 遍历调度队列, 选择第一个 RUNNABLE 进程
    ListNode* node = queue_front(&sched_queue);
    for (;;) {
        auto node_next = node->next;
        auto p = container_of(node, Proc, schinfo.sched_node);

        if (p->state == RUNNABLE) {
            // 将该进程 移动到 队列尾
            _queue_detach(&sched_queue, node);
            _queue_push(&sched_queue, node);
            queue_unlock(&sched_queue);
            return p;
        }

        // 如果遍历完所有进程, 则break
        // 否则继续遍历下一个进程
        if (node_next != queue_front(&sched_queue))
            node = node_next;
        else
            break;
    }

    queue_unlock(&sched_queue); // 释放调度队列锁

    // 如果没有 RUNNABLE 进程, 则返回idle
    return thiscpu->sched.idle_proc;
}

// 将进程p更新为CPU选择的进程
static void update_this_proc(Proc* p)
{
    auto c = thiscpu;
    c->sched.proc = p;
}

// 接受调度 并将当前进程状态更新为new_state (需持有sched_lock)
void sched(enum procstate new_state)
{
    acquire_spinlock(&proc_lock);

    // 获取当前执行的进程
    Proc* this = thisproc();

    // 如果当前进程带有killed标记, 且new state不为zombie, 则调度器直接返回
    if (this->killed && new_state != ZOMBIE) {
        release_spinlock(&proc_lock);
        return;
    }

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

    release_spinlock(&proc_lock);

    // 由进程负责释放锁 并在返回到调度器之前重新获取锁
    // 将旧上下文压栈 并用this->kcontext保存sp
    // 然后从next->kcontext的sp加载新上下文
    // printk("sched: %d -> %d\n", this->pid, next->pid);
    set_cpu_timer(&sched_timer[cpuid()]);
    if (next != this) {
        attach_pgdir(&next->pgdir); // 切换到进程页表
        swtch(&this->kcontext, next->kcontext);
    }

    // 释放调度锁
    release_sched_lock();
}

// proc.c->start_proc 配置进程入口到这里
u64 proc_entry(void (*entry)(u64), u64 arg)
{
    // 释放调度锁
    release_sched_lock();

    // 设置返回地址为entry
    set_return_addr(entry);

    return arg;
}
