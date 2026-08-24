.section .text
.global _start
.extern oak_fork
.extern oak_write
.extern oak_exit
.extern oak_mkdir
.extern oak_create
.extern oak_write_file
.extern oak_read_file

_start:
    call oak_fork
    test %rax, %rax
    jle 1f
    mov $parent_message, %edi
    mov $parent_message_end - parent_message, %esi
    call oak_write
    xor %edi, %edi
    call oak_exit
1:
    mov $file_path, %edi
    call oak_create
    mov $file_path, %edi
    mov $file_text, %esi
    mov $file_text_end - file_text, %edx
    call oak_write_file
    sub $32, %rsp
    mov $file_path, %edi
    mov %rsp, %rsi
    mov $32, %edx
    call oak_read_file
    mov %rsp, %rdi
    mov %rax, %rsi
    call oak_write
    add $32, %rsp
    mov $child_message, %edi
    mov $child_message_end - child_message, %esi
    call oak_write
    mov $1, %edi
    call oak_exit
2:
    pause
    jmp 2b

.section .rodata
parent_message:
    .ascii "Oak OS user fork parent: OK\n"
parent_message_end:
child_message:
    .ascii "Oak OS user fork child: OK\n"
child_message_end:
file_path:
    .ascii "/tmp/hello"
file_path_end:
    .byte 0
file_text:
    .ascii "Oak VFS user file OK\n"
file_text_end:

.section .note.GNU-stack,"",@progbits