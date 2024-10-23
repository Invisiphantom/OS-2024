#include <aarch64/trap.h>
#include <aarch64/intrinsic.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <driver/interrupt.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>

// trap.S->trap_entry 跳转到这里
void trap_global_handler(UserContext* context)
{
    auto p = thisproc();
    p->ucontext = context;

    u64 esr = arch_get_esr();     // Exception Syndrome Reg
    u64 ec = esr >> ESR_EC_SHIFT; // Exception Class
    u64 il = esr >> ESR_IR_SHIFT; // Instruction Length (0:16-bit 1:32-bit)
    u64 iss = esr & ESR_ISS_MASK; // Instruction Specific Syndrome

    (void)iss;

    arch_reset_esr();

    switch (ec) {

        case ESR_EC_UNKNOWN: {
            if (il)
                PANIC();
            else
                interrupt_global_handler();
        } break;

        case ESR_EC_SVC64: {
            syscall_entry(context);
        } break;

        case ESR_EC_IABORT_EL0:
        case ESR_EC_IABORT_EL1:
        case ESR_EC_DABORT_EL0:
        case ESR_EC_DABORT_EL1: {
            printk("Page fault\n");
            PANIC();
        } break;

        default: {
            printk("Unknwon exception %llu\n", ec);
            PANIC();
        }
    }

    // TODO: 如果进程有终止标志，且即将返回到用户态 则执行exit(-1)
    auto spsr_mode = context->spsr_el1 & 0xF;
    if (p->killed && spsr_mode == 0)
        exit(-1);
}

NO_RETURN void trap_error_handler(u64 type)
{
    printk("Unknown trap type %llu\n", type);
    PANIC();
}