#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <driver/uart.h>
#include <kernel/core.h>
#include <kernel/cpu.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <driver/interrupt.h>
#include <kernel/proc.h>
#include <driver/gicv3.h>
#include <driver/timer.h>
#include <aarch64/mmu.h>

static volatile bool boot_secondary_cpus = false;

void main() {
    if (cpuid() == 0) {
        // 清空 bss 段
        extern char edata[], end[];
        memset(edata, 0, (usize)(end - edata));

        init_interrupt();  // 初始化每种中断的处理函数

        uart_init();    // 初始化终端 (UART)
        printk_init();  // 初始化printk

        gicv3_init();         // 初始化GICv3
        gicv3_init_percpu();  // 当前CPU 设置GICv3

        timer_init(1000);     // 初始化定时器
        timer_init_percpu();  // 当前CPU 设置定时器

        kinit();  // 初始化内核内存分配器

        init_sched();  // 初始化调度器
        init_kproc();  // 初始化第一个内核进程 (root_proc)

        smp_init();  // 初始化多核

        arch_fence();  // 内存屏障
        boot_secondary_cpus = true;
    }

    else {
        while (!boot_secondary_cpus)
            ;

        arch_fence();         // 内存屏障
        timer_init_percpu();  // 当前CPU 设置定时器
        gicv3_init_percpu();  // 当前CPU 设置GICv3
    }

    // 设置跳转入口为idle_entry
    set_return_addr(idle_entry);
}