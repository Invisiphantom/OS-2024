

/* `exception_vector.S` send all traps here. */

.global trap_entry
trap_entry:

    /* Save registers */
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!

    mrs x3, elr_el1
    mrs x2, spsr_el1
    mrs x1, sp_el0
    stp x3, x0, [sp, #-16]!
    stp x1, x2, [sp, #-16]!

    /* Call trap(UserContext* context) */
    mov x0, sp
    bl trap_global_handler

.global trap_ret
trap_ret:

    /* Restore registers */
    ldp x1, x2, [sp], #16
    ldp x3, x0, [sp], #16
    msr sp_el0, x1
    msr spsr_el1, x2
    msr elr_el1, x3

    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    eret

// 实际上的 userret
.global trap_return
trap_return:

    mov sp, x0
    b trap_ret