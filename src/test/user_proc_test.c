#include <test/test.h>
#include <common/rc.h>
#include <kernel/pt.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <common/sem.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <driver/memlayout.h>
#include <kernel/sched.h>

PTEntriesPtr get_pte(struct pgdir* pgdir, u64 va, bool alloc);

void vm_test()
{
    printk("vm_test\n");

    static void* p[100000];

    // 记录当前已分配的页数
    extern RefCount kalloc_page_cnt;
    int p0 = kalloc_page_cnt.count;

    // 创建并初始化空页表
    struct pgdir pg;
    init_pgdir(&pg);

    for (u64 i = 0; i < 100000; i++) {
        // 分配新页
        p[i] = kalloc_page();

        // 配置页表的用户虚拟地址映射
        // pgdir=pg, va=i<<12, alloc=true
        // pa=K2P(p[i]), flags=PTE_USER_DATA
        *get_pte(&pg, i << 12, true) = K2P(p[i]) | PTE_USER_DATA;

        // 在页内写入数据 (内核虚拟地址)
        *(int*)p[i] = i;
    }

    // 启用低地址页表映射pg
    attach_pgdir(&pg);

    for (u64 i = 0; i < 100000; i++) {
        // 验证页表项物理地址 处的值为i
        ASSERT(*(int*)(P2K(PTE_ADDRESS(*get_pte(&pg, i << 12, false)))) == (int)i);
        // 验证用户虚拟地址 处的值为i
        ASSERT(*(int*)(i << 12) == (int)i);
    }

    // 递归地释放页表页
    free_pgdir(&pg);

    // 关闭低地址页表映射
    attach_pgdir(&pg);

    // 释放所有内存页
    for (u64 i = 0; i < 100000; i++)
        kfree_page(p[i]);

    // 确保使用的所有页都被释放
    ASSERT(kalloc_page_cnt.count == p0);
    printk("vm_test PASS\n");
}

void trap_return(u64);

static u64 proc_cnt[22] = { 0 }, cpu_cnt[4] = { 0 };
static Semaphore myrepot_done;

// 自定义的系统调用
// syscall.c->syscall_entry 跳转到这里
u64 myreport(u64 id)
{
    static bool stop;

    ASSERT(id < 22);

    if (stop)
        return 0;

    proc_cnt[id]++;
    cpu_cnt[cpuid()]++;

    // 如果条件满足, 停止测试 并唤醒myrepot_done
    if (proc_cnt[id] > 12345) {
        stop = true;
        post_sem(&myrepot_done);
    }

    return 0;
}

void user_proc_test()
{
    printk("user_proc_test\n");

    // 初始化信号量myrepot_done
    init_sem(&myrepot_done, 0);

    extern char loop_start[], loop_end[];

    int pids[22];

    // 初始化22个用户进程 执行loop.S
    for (int i = 0; i < 22; i++) {
        // 创建并初始化新进程
        auto p = create_proc();

        // 将loop.S映射到用户空间EXTMEM
        for (u64 q = (u64)loop_start; q < (u64)loop_end; q += PAGE_SIZE) {
            // pgdir=p->pgdir, va=EXTMEM + q - (u64)loop_start, alloc=true
            // pa=K2P(q), flags=PTE_USER_DATA
            *get_pte(&p->pgdir, EXTMEM + q - (u64)loop_start, true)
                = K2P(q) | PTE_USER_DATA;
        }

        // 确保页表已分配
        ASSERT(p->pgdir.pt);

        // 设置用户上下文 (用于中断返回)
        p->ucontext->x0 = i;           // loop_start(i)
        p->ucontext->elr_el1 = EXTMEM; // loop.S
        p->ucontext->spsr_el1 = 0;     // EL0t

        // 设置跳转到 trap.S->trap_return(p->ucontext)
        pids[i] = start_proc(p, trap_return, (u64)p->ucontext);
        printk("pid[%d] = %d\n", i, pids[i]);
    }

    // 等待某个进程唤醒myrepot_done
    ASSERT(wait_sem(&myrepot_done));
    printk("done\n");

    // 设置所有子进程的终止标志
    for (int i = 0; i < 22; i++)
        ASSERT(kill(pids[i]) == 0);

    // 等待所有子进程trap后执行exit(-1)终止
    for (int i = 0; i < 22; i++) {
        int code;
        int pid = wait(&code);
        printk("pid %d killed\n", pid);
        ASSERT(code == -1);
    }

    // 确认是否负载均衡
    printk("user_proc_test PASS\nRuntime:\n");

    // 打印每个CPU的工作量
    for (int i = 0; i < 4; i++)
        printk("CPU %d: %llu\n", i, cpu_cnt[i]);

    // 打印每个进程的工作量 (确保均匀)
    for (int i = 0; i < 22; i++)
        printk("Proc %d: %llu\n", i, proc_cnt[i]);
}
