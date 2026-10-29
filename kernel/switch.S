/* Переключение контекста x86-64.
   Сохраняем callee-saved регистры (System V ABI) + RSP.
   rdi = &old->rsp
   rsi =  new->rsp
*/
.section .text
.global task_switch
.global task_trampoline_asm

task_switch:
    /* Сохранить контекст текущего потока */
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, (%rdi)        /* old->rsp = rsp */

    /* Загрузить контекст нового потока */
    mov %rsi, %rsp          /* rsp = new->rsp */

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret                     /* прыжок на сохранённый RIP */

/* Точка входа новой задачи: на стеке лежит адрес task_entry_wrapper */
task_trampoline_asm:
    ret

.section .note.GNU-stack,"",@progbits
