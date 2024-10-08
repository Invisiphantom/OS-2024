#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <aarch64/mmu.h>
#include <common/list.h>
#include <common/string.h>
#include <kernel/printk.h>
#include <kernel/cpu.h>

Proc root_proc;

// 全局进程锁
SpinLock proc_lock;

int nextpid = 1;

void kernel_entry();

void idle_func()
{
    for (;;) {
        acquire_sched_lock();
        sched(RUNNABLE);
    }
}

// TODO 初始化第一个内核进程
void init_kproc()
{
    init_spinlock(&proc_lock);

    // 初始化root_proc
    init_proc(&root_proc);
    root_proc.parent = &root_proc;
    start_proc(&root_proc, kernel_entry, 123456);

    // 初始化每个CPU的idle进程
    for (int i = 0; i < NCPU; i++) {
        auto p = create_proc();
        p->idle = true;
        p->state = RUNNING;
        cpus[i].sched.proc = p;
        cpus[i].sched.idle_proc = p;
    }
}

// TODO: 初始化进程
void init_proc(Proc* p)
{
    acquire_spinlock(&proc_lock);

    p->killed = false;
    p->idle = false;

    p->pid = nextpid;
    nextpid = nextpid + 1;

    p->exitcode = 0;
    p->state = UNUSED;

    init_sem(&p->childexit, 0);
    init_list_node(&p->children);
    init_list_node(&p->ptnode);

    // TODO 初始化调度信息
    init_schinfo(&p->schinfo);

    // 栈从高地址向低地址增长
    p->kstack = kalloc_page() + PAGE_SIZE;
    p->kcontext = kalloc_page() + PAGE_SIZE - sizeof(KernelContext);

    release_spinlock(&proc_lock);
}

Proc* create_proc()
{
    Proc* p = kalloc(sizeof(Proc));
    init_proc(p);

    // DEBUG
    auto ps = (u64)p % 0x10000;
    printk("pid=%d - addr=0x%p - schnode=0x%p - ptnode=0x%p\n", p->pid, (void*)ps,
        (void*)(ps + 0x50), (void*)(ps + 0x38));

    return p;
}

// TODO: 设置proc的父进程为thisproc
void set_parent_to_this(Proc* proc)
{
    acquire_spinlock(&proc_lock);

    auto p = thisproc();
    proc->parent = p;
    _insert_into_list(&p->children, &proc->ptnode);

    release_spinlock(&proc_lock);
}

// TODO: 启动进程
int start_proc(Proc* p, void (*entry)(u64), u64 arg)
{
    acquire_spinlock(&proc_lock);

    if (p->parent == NULL) {
        p->parent = &root_proc;
        _insert_into_list(&root_proc.children, &p->ptnode);
    }

    // 设置swtch返回到 proc_entry(entry, arg)
    p->kcontext->x30 = (u64)proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;

    release_spinlock(&proc_lock);

    // 将进程状态设置为 RUNNABLE
    // 并将其添加到调度队列
    activate_proc(p);

    return p->pid;
}

// TODO: 等待子进程退出
// 如果没有子进程，则返回 -1
// 保存退出状态到exitcode 并返回其pid
int wait(int* exitcode)
{
    acquire_spinlock(&proc_lock);

    // 如果没有子进程，则返回-1
    if (_empty_list(&thisproc()->children)) {
        release_spinlock(&proc_lock);
        return -1;
    }

    auto p = thisproc();
    for (;;) {

        ListNode* pp_node = p->children.next; // 遍历所有子进程
        for (;;) {
            auto pp_node_next = pp_node->next;
            auto pp = container_of(pp_node, Proc, ptnode);

            // 如果子进程已经退出, 则释放子进程资源
            if (pp->state == ZOMBIE) {
                int pid = pp->pid;

                if (exitcode != 0)
                    *exitcode = pp->exitcode;

                // 释放子进程所有资源
                // (栈从高地址向低地址增长)
                // 释放内核栈, 用户进程上下文栈, 内核进程上下文栈

                _detach_from_list(&pp->ptnode);

                kfree_page((void*)round_up((u64)pp->kstack - PAGE_SIZE, PAGE_SIZE));
                kfree_page((void*)round_up((u64)pp->kcontext - PAGE_SIZE, PAGE_SIZE));
                kfree(pp);

                release_spinlock(&proc_lock);
                return pid;
            }

            // 如果遍历完所有子进程, 则等待子进程退出
            // 否则继续遍历下一个子进程
            if (pp_node_next == &p->children)
                break;
            else
                pp_node = pp_node_next;
        }

        // 释放锁 并在sleeplist上休眠, 醒来时重新获取锁
        release_spinlock(&proc_lock);
        wait_sem(&p->childexit);
        acquire_spinlock(&proc_lock);
    }

    printk("wait: should not reach here");
    PANIC();
}

// TODO: 退出当前进程, 不会返回
// 退出进程会保持ZOMBIE状态, 直到其父进程调用wait回收
NO_RETURN void exit(int code)
{
    auto p = thisproc();

    // 确保不是root_proc进程
    if (p == &root_proc)
        PANIC();

    // 将p的弃子交给root_proc

    if (_empty_list(&p->children) == false)
    {
        ListNode* pp_node = p->children.next;
        for (;;) {
            auto pp_node_next = pp_node->next;

            auto pp = container_of(pp_node, Proc, ptnode);
            pp->parent = &root_proc;
            _insert_into_list(&root_proc.children, &pp->ptnode);
            activate_proc(&root_proc); // 激活root_proc

            if (pp_node_next == &p->children)
                break;
            else
                pp_node = pp_node_next;
        }
    }

    post_sem(&p->parent->childexit); // 释放父进程信号量
    p->exitcode = code;              // 记录退出状态位

    acquire_sched_lock();
    sched(ZOMBIE); // 调度进程

    PANIC(); // prevent the warning of 'no_return function returns'
}
