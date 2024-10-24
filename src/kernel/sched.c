#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <common/rbtree.h>

extern bool panic_flag;

static Queue sched_queue; // 调度队列

// 调度定时器
static bool isSched[NCPU];
static struct timer sched_timer[NCPU];
static void time_sched(struct timer* t) { sched(RUNNABLE); }

extern void swtch(KernelContext** old_ctx, KernelContext* new_ctx);

// 初始化调度器
void init_sched()
{
    queue_init(&sched_queue); // 初始化调度队列

    // 初始化调度定时器
    for (int i = 0; i < NCPU; i++) {
        sched_timer[i].elapse = 20; // 间隔时间
        sched_timer[i].handler = time_sched;
    }
}

// 为每个新进程 初始化自定义的schinfo
void init_schinfo(struct schinfo* info) { init_list_node(&info->sched_node); }

// 返回当前CPU上执行的进程
Proc* thisproc() { return thiscpu->sched.proc; }



// 唤醒进程
// 如果进程状态是 RUNNING/RUNNABLE: 什么都不做
// 如果进程状态是 SLEEPING/UNUSED:  将进程状态设置为 RUNNABLE 并将其添加到调度队列
// 其他情况: panic
bool activate_proc(Proc* p)
{
    acquire_spinlock(&p->lock); // *

    if (p->state == ZOMBIE) {
        release_spinlock(&p->lock); // *
        return false;
    }

    if (p->state == RUNNING || p->state == RUNNABLE) {
        release_spinlock(&p->lock); // *
        return true;
    }

    if (p->state == SLEEPING || p->state == UNUSED) {
        // 更新进程状态设置为 RUNNABLE
        p->state = RUNNABLE;

        // 将其添加到调度队列
        queue_push_lock(&sched_queue, &p->schinfo.sched_node);

        release_spinlock(&p->lock); // *
        return true;
    }

    printk("activate_proc: invalid state %d\n", p->state);
    PANIC();
}

// 更新当前进程的状态为new_state (需持有this->lock)
static void update_this_state(enum procstate new_state)
{
    // 更新状态为new_state
    auto p = thisproc();
    p->state = new_state;

    // 如果new_state=SLEEPING/ZOMBIE, 则从调度队列中移除
    if (new_state == SLEEPING || new_state == ZOMBIE)
        queue_detach_lock(&sched_queue, &p->schinfo.sched_node);
}

// 从调度队列中挑选进程 (获取锁)
static Proc* pick_next()
{
    auto this = thisproc();

    // 如果调度队列中没有进程, 则返回idle
    if (queue_empty(&sched_queue))
        return thiscpu->sched.idle_proc;

    queue_lock(&sched_queue); // *

    // 遍历调度队列, 选择第一个 RUNNABLE 进程
    ListNode* node = queue_front(&sched_queue);
    for (;;) {
        auto node_next = node->next;
        auto p = container_of(node, Proc, schinfo.sched_node);

        if (p != this) {
            acquire_spinlock(&p->lock); // **
            if (p->state == RUNNABLE) {
                // 将该进程 移动到 队列尾
                _queue_detach(&sched_queue, node);
                _queue_push(&sched_queue, node);
                queue_unlock(&sched_queue); // *
                return p;
            }
            release_spinlock(&p->lock); // **
        }

        // 如果遍历完所有进程, 则break
        // 否则继续遍历下一个进程
        if (node_next != queue_front(&sched_queue))
            node = node_next;
        else
            break;
    }

    queue_unlock(&sched_queue); // *

    // 如果没有 RUNNABLE 进程, 则返回idle
    return thiscpu->sched.idle_proc;
}

// 移除可能存在的调度定时器
void acquire_sched() { cancel_cpu_timer(&sched_timer[cpuid()]); }
void release_sched() { }

// 接受调度 并将当前进程状态更新为new_state (需持有sched_lock)
void sched(enum procstate new_state)
{

    // 获取当前执行的进程
    Proc* this = thisproc();
    Proc* before;
    Proc* next;

    // 如果非idle进程, 则切换到 idle进程
    if (this->idle == false) {
        // 该锁在idle进程的 swtch结束后释放
        acquire_spinlock(&this->lock); // *

        // 如果有终止标记, 且新状态不为ZOMBIE, 则调度器直接返回
        if (this->killed && new_state != ZOMBIE)
            return;

        // 确保当前进程是 RUNNING 状态
        ASSERT(this->state == RUNNING);

        // 更新当前进程状态为 new_state
        update_this_state(new_state);

        // 将CPU切换到idle进程
        next = thiscpu->sched.idle_proc;
        thiscpu->sched.proc = next;

        // 记录当前进程, 用于释放锁
        thiscpu->sched.before_proc = this;
        attach_pgdir(&next->pgdir);
        swtch(&this->kcontext, next->kcontext);

        // idle进程执行完毕, 返回到当前进程 (持有锁)
        release_spinlock(&this->lock); // **
    }

    // 如果是idle进程, 则切换到 调度队列首进程
    else {
        for (;;) {
            // 选择下一个进程 (获取锁)
            next = pick_next(); // **

            // 如果还是idle进程, 则退出循环
            if (next == this)
                return;

            // 切换到下一个进程
            ASSERT(next->state == RUNNABLE);
            next->state = RUNNING;
            thiscpu->sched.proc = next;

            // 切换到进程页表
            attach_pgdir(&next->pgdir);

            // 启用调度定时器
            set_cpu_timer(&sched_timer[cpuid()]);

            // 切换进程上下文
            swtch(&this->kcontext, next->kcontext);

            // 进程执行完毕, 返回到idle进程 (持有锁)
            before = thiscpu->sched.before_proc;

            // 该锁在非idle进程的 swtch调用前获得
            release_spinlock(&before->lock); // *
        }
    }
}

// proc.c->start_proc 配置进程入口到这里
u64 proc_entry(void (*entry)(u64), u64 arg)
{
    // 释放在sched()中获取的锁
    auto this = thisproc();
    release_spinlock(&this->lock); // **

    // 设置返回地址为entry
    set_return_addr(entry);

    return arg;
}
