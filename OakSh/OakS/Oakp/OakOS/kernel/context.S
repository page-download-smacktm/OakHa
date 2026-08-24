.section .text
.code64
.global acorn_switch_context
.type acorn_switch_context, @function

acorn_switch_context:
    mov %rsp, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %rbx, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %r13, 32(%rdi)
    mov %r14, 40(%rdi)
    mov %r15, 48(%rdi)
    mov 0(%rsi), %rsp
    mov 8(%rsi), %rbp
    mov 16(%rsi), %rbx
    mov 24(%rsi), %r12
    mov 32(%rsi), %r13
    mov 40(%rsi), %r14
    mov 48(%rsi), %r15
    ret

.section .note.GNU-stack,"",@progbits