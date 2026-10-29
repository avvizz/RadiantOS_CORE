/* Syscall int 0x80 + enter/leave user mode */
.section .text

.global isr128
isr128:
    pushq $0
    pushq $128

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    cld
    call syscall_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq

/* Сохранённый kernel RSP — куда вернуться из user exit */
.section .bss
.align 8
saved_kernel_rsp:
    .quad 0

.section .text

/*
 * enter_user_mode(entry=rdi, user_stack=rsi)
 * Сохраняет callee-saved и RSP, уходит в ring 3.
 * user_exit_to_kernel() возвращает сюда (как ret из этой функции).
 * Возвращаемое значение = exit code в rax.
 */
.global enter_user_mode
enter_user_mode:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, saved_kernel_rsp(%rip)

    cli
    movq %rsi, %rax          /* user RSP */

    pushq $0x23              /* SS */
    pushq %rax               /* RSP */
    pushq $0x202             /* RFLAGS IF */
    pushq $0x1B              /* CS */
    pushq %rdi               /* RIP = entry */

    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es

    iretq

/*
 * user_exit_to_kernel(code=rdi)
 * Вызывается из syscall_handler при SYS_EXIT.
 * Восстанавливает kernel context enter_user_mode и делает ret.
 */
.global user_exit_to_kernel
user_exit_to_kernel:
    cli
    movq %rdi, %rax          /* exit code → return value */

    /* kernel data segments */
    movw $0x10, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %ss

    movq saved_kernel_rsp(%rip), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    sti
    ret

.section .note.GNU-stack,"",@progbits
