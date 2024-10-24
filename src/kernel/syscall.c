#include <kernel/syscall.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <common/sem.h>
#include <test/test.h>
#include <aarch64/intrinsic.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

u64 syscall_myreport()
{
    u64 id = thisproc()->ucontext->x0;
    return myreport(id);
}

// 系统调用函数映射表
static u64 (*syscall_table[NR_SYSCALL])(void) = {
    [0 ... NR_SYSCALL - 1] = NULL,
    [SYS_myreport] = (void*)syscall_myreport,
};

// 处理系统调用
// 调用syscall_table[id] 系统调用编号为x8, 参数为x0-x5, 返回值为x0
// 确保检查id的范围, 如果id >= NR_SYSCALL 则panic
void syscall_entry(UserContext* context)
{
    auto p = thisproc();
    int num = context->x8;

    if (num > 0 && num < NR_SYSCALL && syscall_table[num]) {
        context->x0 = syscall_table[num]();
    } else {
        printk("pid=%d: unknown sys call %d\n", p->pid, num);
        PANIC();
    }
}

#pragma GCC diagnostic pop