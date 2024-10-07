#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <aarch64/mmu.h>

struct cpu cpus[NCPU];

void set_cpu_on() {
    // 关闭中断 并确保中断之前是关闭状态
    ASSERT(_arch_disable_trap() == 0);

    // 将用户页表ttbr0设置为无效页表
    extern PTEntries invalid_pt;
    arch_set_ttbr0(K2P(&invalid_pt));
    
    // 设置vbar异常处理函数地址
    extern char exception_vector[];
    arch_set_vbar(exception_vector);
    
    // 重置esr异常状态信息
    arch_reset_esr();
    
    // 启用当前CPU
    cpus[cpuid()].online = true;
    
    // 打印欢迎信息
    printk("CPU %lld: hello\n", cpuid());
}

void set_cpu_off() {
    // 关闭当前CPU中断
    _arch_disable_trap();

    // 停用当前CPU
    cpus[cpuid()].online = false;
    
    // 打印结束信息
    printk("CPU %lld: stopped\n", cpuid());
}
