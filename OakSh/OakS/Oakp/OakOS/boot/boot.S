.section .multiboot, "a"
.align 4
.long 0x1BADB002
.long 0x00000004
.long -(0x1BADB006)
.long 0
.long 0
.long 0
.long 0
.long 0
.long 0
.long 1024
.long 768
.long 32

.section .text
.code32
.global _start
.type _start, @function
.extern acorn_main
.extern acorn_user_main

_start:
    cli
    mov $stack_top, %esp
    push %ebx
    push %eax
    call setup_long_mode
    hlt

setup_long_mode:
    mov $page_table_l3, %edi
    xor %eax, %eax
    mov $4096, %ecx
    rep stosl
    mov $page_table_l3, %edi
    mov $page_table_l2, %eax
    or $0x7, %eax
    mov %eax, (%edi)
    mov $page_table_l2, %edi
    mov $page_table_l1, %eax
    or $0x7, %eax
    mov %eax, (%edi)
    mov $page_table_l1_1, %eax
    or $0x7, %eax
    mov %eax, 8(%edi)
    mov $page_table_l1_2, %eax
    or $0x7, %eax
    mov %eax, 16(%edi)
    mov $page_table_l1_3, %eax
    or $0x7, %eax
    mov %eax, 24(%edi)
    mov $page_table_l1, %edi
    mov $0x87, %eax
    mov $512, %ecx
1:
    mov %eax, (%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 1b
    mov $page_table_l1_1, %edi
    mov $0x40000087, %eax
    mov $512, %ecx
2:
    mov %eax, (%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 2b
    mov $page_table_l1_2, %edi
    mov $0x80000087, %eax
    mov $512, %ecx
3:
    mov %eax, (%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 3b
    mov $page_table_l1_3, %edi
    mov $0xC0000087, %eax
    mov $512, %ecx
4:
    mov %eax, (%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 4b
    mov $page_table_l3, %eax
    mov %eax, %cr3
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    lgdt gdt64_pointer
    ljmp $0x08, $long_mode_start

.code64
long_mode_start:
    mov $tss64, %rax
    mov %ax, gdt64_tss+2
    shr $16, %rax
    mov %al, gdt64_tss+4
    shr $8, %rax
    mov %al, gdt64_tss+7
    shr $8, %rax
    mov %eax, gdt64_tss+8
    mov $0x67, %ax
    mov %ax, gdt64_tss
    movb $0x89, gdt64_tss+5
    mov $stack_top, %rax
    mov %rax, tss64+4
    lgdt gdt64_pointer
    mov $0x28, %ax
    ltr %ax
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov 4(%rsp), %edi
    mov 8(%rsp), %esi
    call acorn_main
2:
    hlt
    jmp 2b

.global acorn_enter_user
.type acorn_enter_user, @function
acorn_enter_user:
    mov %rsi, %rax
    pushq $0x23
    push %rax
    pushfq
    pop %rax
    or $0x200, %rax
    push %rax
    pushq $0x1B
    push %rdi
    iretq

.align 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
    .quad 0x00AFFA000000FFFF
    .quad 0x00AFF2000000FFFF
gdt64_tss:
    .skip 16
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

.section .bss
.align 4096
page_table_l3: .skip 4096
page_table_l2: .skip 4096
page_table_l1: .skip 4096
page_table_l1_1: .skip 4096
page_table_l1_2: .skip 4096
page_table_l1_3: .skip 4096
.align 16
stack_bottom: .skip 16384
stack_top:

tss64: .skip 104

.section .note.GNU-stack,"",@progbits
