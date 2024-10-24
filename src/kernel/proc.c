#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <aarch64/mmu.h>
#include <common/list.h>
#include <common/string.h>
#include <kernel/printk.h>
#include <kernel/cpu.h>

#include <driver/memlayout.h>
#include <kernel/pt.h>

Proc root_proc;      // 初始init进程
void kernel_entry(); // root_proc 进程跳转到这里

// 顺序分配pid
static int nextpid = 1;

// pid树
static struct rb_root_ pid_root;
static bool __pid_cmp(rb_node lnode, rb_node rnode)
{
    return container_of(lnode, Proc, _node)->pid < container_of(rnode, Proc, _node)->pid;
}

// 初始化第一个内核进程
void init_kproc()
{
    // 初始化pid树
    rb_init(&pid_root);

    // 初始化每个CPU的idle进程 (1-4)
    for (int i = 0; i < NCPU; i++) {
        auto p = create_proc();
        p->idle = true;
        p->state = RUNNING;
        cpus[i].sched.proc = p;
        cpus[i].sched.idle_proc = p;
    }

    // 初始化root_proc (5)
    init_proc(&root_proc);
    root_proc.parent = &root_proc;
    start_proc(&root_proc, kernel_entry, 123456);
}

// 初始化新分配的进程
void init_proc(Proc* p)
{

    init_spinlock(&p->lock);

    p->killed = false;
    p->idle = false;

    // 将进程插入pid树
    p->pid = nextpid;
    nextpid = nextpid + 1;
    ASSERT(0 == rb_insert_lock(&p->_node, &pid_root, __pid_cmp));

    p->exitcode = 0;
    p->state = UNUSED;

    init_sem(&p->childexit, 0);
    init_list_node(&p->children);
    init_list_node(&p->ptnode);

    // 初始化调度队列结点
    init_schinfo(&p->schinfo);

    // 初始化进程页表为空
    init_pgdir(&p->pgdir);

    // 栈从高地址向低地址增长
    p->kcontext = kalloc_page() + PAGE_SIZE - sizeof(KernelContext);
    p->ucontext = kalloc_page() + PAGE_SIZE - sizeof(UserContext);

    // TODO: 因为trap_ret会将ucontext加载完, 所以直接将sp_el0设置为用户栈底
    p->ucontext->sp_el0 = round_up((u64)p->ucontext, PAGE_SIZE);
}

Proc* create_proc()
{
    Proc* p = kalloc(sizeof(Proc));
    init_proc(p);

    // 打印每个新进程的信息 (FOR DEBUG)
    // addr:进程结构体地址  schnode:调度结点地址  ptnode:串在父进程链表上的结点地址
    // auto p_mask = (u64)p % 0x10000;
    // printk("pid=%d - addr=0x%p - schnode=0x%p - ptnode=0x%p\n", p->pid, (void*)p_mask,
    //     (void*)(p_mask + 0x50), (void*)(p_mask + 0x38));

    return p;
}

// 设置proc的父进程为thisproc
void set_parent_to_this(Proc* proc)
{

    auto p = thisproc();

    acquire_spinlock(&proc->lock); // *
    proc->parent = p;
    release_spinlock(&proc->lock); // *

    acquire_spinlock(&p->lock); // *
    _insert_into_list(&p->children, &proc->ptnode);
    release_spinlock(&p->lock); // *
}

// 配置进程的初始上下文指向 proc_entry(entry, arg)
// 激活进程, 并将其添加到调度队列
int start_proc(Proc* p, void (*entry)(u64), u64 arg)
{
    acquire_spinlock(&p->lock); // *

    // 如果进程没有父进程, 则将其父进程设置为root_proc
    if (p->parent == NULL) {
        p->parent = &root_proc;
        acquire_spinlock(&root_proc.lock); // *
        _insert_into_list(&root_proc.children, &p->ptnode);
        release_spinlock(&root_proc.lock); // *
    }

    // 设置swtch返回后跳转到 proc_entry(entry, arg)
    p->kcontext->x30 = (u64)proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;

    release_spinlock(&p->lock); // *

    // 将进程状态设置为 RUNNABLE
    // 并将其添加到调度队列
    activate_proc(p);

    return p->pid;
}

// 等待子进程退出
// 如果没有子进程，则返回 -1
// 保存退出状态到exitcode 并返回其pid
int wait(int* exitcode)
{
    auto p = thisproc();
    acquire_spinlock(&p->lock); // *

    // 如果没有子进程，则返回-1
    if (_empty_list(&p->children)) {
        release_spinlock(&p->lock); // *
        return -1;
    }

    for (;;) {

        // 开始遍历所有子进程
        for (auto pp_node = p->children.next;;) {
            // 暂存下个结点, 防止detach后找不到
            auto pp_node_next = pp_node->next;
            auto pp = container_of(pp_node, Proc, ptnode);

            // 如果子进程已经退出, 则释放子进程资源
            acquire_spinlock(&pp->lock); // *
            if (pp->state == ZOMBIE) {
                // 从pid树中移除
                int pid = pp->pid;
                _rb_erase(&pp->_node, &pid_root);

                // 保存退出状态
                if (exitcode != 0)
                    *exitcode = pp->exitcode;

                // 将子进程 从父进程的子进程链表中移除
                _detach_from_list(&pp->ptnode);

                // 递归释放页表页映射
                free_pgdir(&pp->pgdir);

                // 释放进程栈 (栈从高地址向低地址增长)
                kfree_page((void*)round_down((u64)pp->kcontext - 1, PAGE_SIZE));
                kfree_page((void*)round_down((u64)pp->ucontext - 1, PAGE_SIZE));

                // 释放进程结构体
                release_spinlock(&pp->lock); // *
                kfree(pp);

                release_spinlock(&p->lock); // *
                return pid;
            }
            release_spinlock(&pp->lock); // *

            // 如果遍历完所有子进程, 则等待子进程退出
            if (pp_node_next == &p->children)
                break;

            // 否则继续遍历下一个子进程
            else
                pp_node = pp_node_next;
        }

        // 释放锁 并在sleeplist上休眠, 醒来时重新获取锁
        release_spinlock(&p->lock); // *
        wait_sem(&p->childexit);
        acquire_spinlock(&p->lock); // *
    }

    printk("wait: should not reach here");
    PANIC();
}

// 退出当前进程, 不会返回
// 退出进程会保持ZOMBIE状态, 直到其父进程调用wait回收
NO_RETURN void exit(int code)
{
    auto p = thisproc();

    // 确保不是root_proc进程
    if (p == &root_proc)
        PANIC();

    // 如果进程p有孩子, 则将这些弃子交给root_proc
    acquire_spinlock(&p->lock); // *

    if (_empty_list(&p->children) == false) {
        for (ListNode* pp_node = p->children.next;;) {
            // 暂存下个结点, 防止insert后找不到
            auto pp_node_next = pp_node->next;
            auto pp = container_of(pp_node, Proc, ptnode);

            // 设置root_proc为义父
            acquire_spinlock(&pp->lock); // **
            pp->parent = &root_proc;
            {
                acquire_spinlock(&root_proc.lock); // ***
                _insert_into_list(&root_proc.children, &pp->ptnode);
                release_spinlock(&root_proc.lock); // ***
            }
            release_spinlock(&pp->lock); // **

            // 激活root_proc进程
            activate_proc(&root_proc);

            // 如果遍历完所有子进程, 则break
            if (pp_node_next == &p->children)
                break;

            // 否则继续遍历下一个子进程
            else
                pp_node = pp_node_next;
        }
    }

    post_sem(&p->parent->childexit); // 释放父进程信号量
    p->exitcode = code;              // 记录退出状态位
    release_spinlock(&p->lock);      // *

    // 调度进程 状态切换为ZOMBIE
    acquire_sched_lock();
    sched(ZOMBIE);

    printk("exit: should not reach here");
    PANIC();
}

// 遍历进程树, 终止进程
// 设置进程的终止标志位, 并返回0
// 如果pid无效 (找不到进程)  则返回-1
int kill(int pid)
{

    // 确保不是root_proc和idle进程
    ASSERT(pid > 1 + NCPU);

    // 从pid树中查找进程
    Proc pid_p = { .pid = pid };
    auto node_p = rb_lookup_lock(&pid_p._node, &pid_root, __pid_cmp);
    if (node_p == NULL)
        return -1;

    auto p = container_of(node_p, Proc, _node);

    acquire_spinlock(&p->lock); // *
    p->killed = true;
    release_spinlock(&p->lock); // *

    // 唤醒如果在睡眠的进程
    activate_proc(p);

    return 0;
}