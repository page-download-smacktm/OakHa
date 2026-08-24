.section .text
.code64
.global acorn_interrupt_stub
.type acorn_interrupt_stub, @function
.extern acorn_exception
.extern acorn_timer_irq
.extern acorn_keyboard_irq
.extern acorn_mouse_irq
.extern acorn_syscall_dispatch_entry
.extern acorn_user_exit

acorn_interrupt_stub:
    cli
    call acorn_exception
1:
    hlt
    jmp 1b

.global acorn_timer_stub
acorn_timer_stub:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbx
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    call acorn_timer_irq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

.global acorn_keyboard_stub
acorn_keyboard_stub:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbx
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    call acorn_keyboard_irq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

.global acorn_syscall_stub
acorn_syscall_stub:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbx
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov 112(%rsp), %rdi
    mov 80(%rsp), %rsi
    mov 88(%rsp), %rdx
    mov 96(%rsp), %rcx
    cmpq $4, 112(%rsp)
    jne 1f
    mov $1, %r12d
    jmp 2f
1:
    xor %r12d, %r12d
2:
    call acorn_syscall_dispatch_entry
    mov %rax, 112(%rsp)
    test %r12d, %r12d
    jnz .Lsyscall_exit
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

.global acorn_mouse_stub
acorn_mouse_stub:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbx
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    call acorn_mouse_irq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

.Lsyscall_exit:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rbx
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    add $48, %rsp
    call acorn_user_exit

.global acorn_load_idt
.type acorn_load_idt, @function
acorn_load_idt:
    lidt (%rdi)
    ret

.section .note.GNU-stack,"",@progbits
