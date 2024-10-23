#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <test/test.h>

volatile bool panic_flag;

// lab1
// NO_RETURN void idle_entry() {
//     kalloc_test();
//     arch_stop_cpu();
// }

// main 函数跳转到这里
NO_RETURN void idle_entry()
{
    set_cpu_on();

    while (1) {
        acquire_sched_lock();
        sched(RUNNABLE);

        if (panic_flag)
            break;

        arch_with_trap { arch_wfi(); }
    }

    set_cpu_off();
    arch_stop_cpu();
}

// root_proc 进程跳转到这里
NO_RETURN void kernel_entry()
{
    printk("Hello world! (Core %lld)\n", cpuid());


    
    // lab2
    proc_test();

    // vm_test();

    // user_proc_test();



    while (1) {
        acquire_sched_lock();
        sched(RUNNABLE);
    }
}

NO_INLINE NO_RETURN void _panic(const char* file, int line)
{
    printk("===== %s:%d PANIC cpu%lld =====\n", file, line, cpuid());

    // 设置panic标志, 通知其他CPU停止
    panic_flag = true;

    // 关闭当前CPU
    set_cpu_off();

    // 等待所有CPU停止
    for (int i = 0; i < NCPU; i++) {
        if (cpus[i].online)
            i--;
    }

    printk("Kernel PANIC invoked at %s:%d. Stopped.\n", file, line);
    arch_stop_cpu();
}