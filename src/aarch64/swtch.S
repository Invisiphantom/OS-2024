

// void swtch(KernelContext** old_ctx, KernelContext* new_ctx)
// 将旧上下文压栈 并用old_ctx存放栈地址寄存器sp
// 从new_ctx加载新sp 并弹出新上下文

.global swtch

swtch:
    # Save old callee-saved registers
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x0, x1, [sp, #-16]!

    # Save old sp
    mov x19, sp
    str x19, [x0]

    # Load new sp
    mov sp, x1

    # Load new callee-saved registers
    ldp x0, x1, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    ret
