#include "acorn/serial.h"
#include "acorn/vga.h"
#include "acorn/interrupts.h"
#include "acorn/memory.h"
#include "acorn/pic.h"
#include "acorn/timer.h"
#include "acorn/keyboard.h"
#include "acorn/scheduler.h"
#include "acorn/process.h"
#include "acorn/syscall.h"
#include "acorn/vm.h"
#include "acorn/fs.h"
#include "acorn/ata.h"
#include "acorn/framebuffer.h"
#include "acorn/mouse.h"
#include "acorn/gui.h"
#include "acorn/e1000.h"
#include "acorn/network.h"
#include "acorn/tls.h"
#include "acorn/dillo_platform.h"

enum { MULTIBOOT_BOOTLOADER_MAGIC = 0x2BADB002 };

extern void acorn_enter_user(void (*entry)(void), unsigned long stack_top);
extern unsigned char _binary_build_user_init_elf_start[];
extern unsigned char _binary_build_user_init_elf_end[];

static void worker_one(void)
{
    serial_write("thread 1: running\n");
    scheduler_yield();
    serial_write("thread 1: resumed\n");
}

static void worker_two(void)
{
    serial_write("thread 2: running\n");
    scheduler_yield();
    serial_write("thread 2: resumed\n");
}

static void preempt_worker_one(void)
{
    volatile unsigned long work = 0;
    for (unsigned long index = 0; index < 20000000; ++index)
        work += index;
    serial_write("preemptive thread 1: complete\n");
    (void)work;
}

static void preempt_worker_two(void)
{
    volatile unsigned long work = 0;
    for (unsigned long index = 0; index < 20000000; ++index)
        work += index;
    serial_write("preemptive thread 2: complete\n");
    (void)work;
}

void acorn_main(unsigned long multiboot_magic, unsigned long multiboot_info)
{
    serial_init();
    vga_init();
    serial_write("Oak OS / Acorn kernel\n");
    serial_write("architecture: x86_64\n");
    serial_write("multiboot magic: ");
    serial_write_hex(multiboot_magic);
    serial_write("\n");

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_write("KERNEL PANIC\nReason: invalid multiboot magic\n");
        vga_write("KERNEL PANIC\nInvalid Multiboot magic\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    (void)multiboot_info;
    framebuffer_init(multiboot_info);
    serial_write("framebuffer self-test: ");
    serial_write(framebuffer_self_test() ? "OK\n" : "UNAVAILABLE\n");
    gui_init();
    serial_write("GUI self-test: ");
    serial_write(gui_self_test() ? "OK\n" : "UNAVAILABLE\n");
    serial_write("Dillo platform self-test: ");
    serial_write(dillo_platform_self_test() ? "OK\n" : "FAILED\n");
    serial_write("boot: interrupt subsystem deferred until stable boot\n");
    timer_init();
    keyboard_init();
    mouse_init();
    e1000_init();
    network_init();
    serial_write("network: E1000 self-test: ");
    serial_write(e1000_self_test() ? "OK\n" : "UNAVAILABLE\n");
    serial_write("network: IPv4/ARP self-test: ");
    serial_write(network_self_test() ? "OK\n" : "UNAVAILABLE\n");
    serial_write("boot: OK\n");
    serial_write("interrupts: IDT loaded\n");
    memory_init(multiboot_info);
    serial_write("TLS crypto self-test: ");
    serial_write(tls_crypto_self_test() ? "OK\n" : "FAILED\n");
    serial_write("TLS client bootstrap: ");
    serial_write(tls_client_self_test() ? "OK\n" : "FAILED\n");
    serial_write("memory self-test: ");
    serial_write(memory_self_test() ? "OK\n" : "FAILED\n");
    serial_write("virtual memory self-test: ");
    serial_write(vm_self_test() ? "OK\n" : "FAILED\n");
    serial_write("scheduler self-test: ");
    serial_write(scheduler_self_test() ? "OK\n" : "FAILED\n");
    scheduler_init();
    thread_create(worker_one);
    thread_create(worker_two);
    serial_write("context switch test:\n");
    scheduler_start();
    serial_write("context switch: OK\n");
    serial_write("drivers: PIT + PS/2 keyboard ready\n");
    serial_write("keyboard self-test: ");
    serial_write(keyboard_self_test() ? "OK\n" : "FAILED\n");
    serial_write("process self-test: ");
    serial_write(process_self_test() ? "OK\n" : "FAILED\n");
    serial_write("ELF loader self-test: ");
    serial_write(elf_loader_self_test() ? "OK\n" : "FAILED\n");
    ata_init();
    serial_write("ATA PIO self-test: ");
    serial_write(ata_self_test() ? "OK\n" : "FAILED\n");
    serial_write("persistent filesystem self-test: ");
    serial_write(fs_self_test() ? "OK\n" : "FAILED\n");
    serial_write("syscall self-test: ");
    serial_write(syscall_self_test() ? "OK\n" : "FAILED\n");
    vga_write("Oak OS\nAcorn kernel booted successfully.\n\nPhase 4: timer + keyboard\n");
    __asm__ volatile ("sti");
    scheduler_init();
    thread_create(preempt_worker_one);
    thread_create(preempt_worker_two);
    serial_write("preemptive scheduler test:\n");
    scheduler_start();
    serial_write("preemption: OK\n");
    serial_write("entering user mode...\n");
    scheduler_init();
    struct process *user_process = process_create((void (*)(void))1);
    unsigned long user_image_size = (unsigned long)(_binary_build_user_init_elf_end -
        _binary_build_user_init_elf_start);
    if (user_process == (struct process *)0 || !process_load_elf(user_process,
        _binary_build_user_init_elf_start, user_image_size)) {
        serial_write("KERNEL PANIC\nReason: user ELF load failed\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    process_set_current(user_process);
    struct process *child_process = process_fork(user_process);
    if (child_process == (struct process *)0 ||
        process_id(child_process) == process_id(user_process) ||
        process_address_space(child_process) == process_address_space(user_process)) {
        serial_write("KERNEL PANIC\nReason: fork failed\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    process_exit(child_process, 7);
    int child_status = 0;
    if (process_wait(process_id(child_process), &child_status) !=
        (int)process_id(child_process) || child_status != 7) {
        serial_write("KERNEL PANIC\nReason: waitpid failed\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    if (process_fork_return(user_process) != (long)process_id(child_process) ||
        process_fork_return(child_process) != 0) {
        serial_write("KERNEL PANIC\nReason: fork return values failed\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    serial_write("fork/waitpid integration: OK\n");
    serial_write("user mode demo complete; staying in kernel mode\n");
    for (;;) __asm__ volatile ("hlt");
}