#include <kernel/syscall.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <common/sem.h>
#include <test/test.h>
#include <aarch64/intrinsic.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

// 系统调用函数映射表
void* syscall_table[NR_SYSCALL] = {
    [0 ... NR_SYSCALL - 1] = NULL,
    [SYS_myreport] = (void*)syscall_myreport,
};

// TODO: 处理系统调用
// 调用syscall_table[id] 系统调用编号为x8, 参数为x0-x5, 返回值为x0
// 确保检查id的范围, 如果id >= NR_SYSCALL 则panic
void syscall_entry(UserContext* context) { }

#pragma GCC diagnostic pop