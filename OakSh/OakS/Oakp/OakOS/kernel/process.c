#include "acorn/process.h"
#include "acorn/memory.h"
#include "acorn/scheduler.h"
#include "acorn/vm.h"

enum { MAX_PROCESSES = 32 };

extern int elf_load(struct address_space *space, const void *image,
    unsigned long image_size, unsigned long *entry);
extern void acorn_enter_user(void (*entry)(void), unsigned long stack_top);

struct process {
    unsigned long pid;
    unsigned long uid;
    unsigned long gid;
    struct thread *main_thread;
    struct address_space *address_space;
    unsigned long entry;
    unsigned long stack_top;
    enum process_state state;
    int exit_status;
    struct process *parent;
    const void *image;
    unsigned long image_size;
    long fork_return;
};

static unsigned long next_pid = 1;
static struct process *current_process;
static struct process *process_table[MAX_PROCESSES];
static unsigned long process_count;

static void process_user_thread(void)
{
    struct process *process = (struct process *)scheduler_current_owner();
    process_set_current(process);
    vm_activate_root(process_address_space(process));
    acorn_enter_user((void (*)(void))process_entry(process),
        process_stack_top(process));
}

struct process *process_create(void (*entry)(void))
{
    if (entry == (void (*)(void))0 || process_count == MAX_PROCESSES)
        return (struct process *)0;
    struct process *process = (struct process *)kmalloc(sizeof(struct process));
    if (process == (struct process *)0) return (struct process *)0;
    process->main_thread = thread_create(entry);
    if (process->main_thread == (struct thread *)0) return (struct process *)0;
    process->address_space = address_space_create();
    if (process->address_space == (struct address_space *)0) return (struct process *)0;
    process->stack_top = 0x40100000;
    process->uid = 0;
    process->gid = 0;
    process->state = PROCESS_RUNNING;
    process->exit_status = 0;
    process->parent = current_process;
    process->image = (const void *)0;
    process->image_size = 0;
    process->fork_return = -1;
    process_table[process_count++] = process;
    process->pid = next_pid++;
    return process;
}

struct process *process_current(void)
{
    return current_process;
}

int process_set_current(struct process *process)
{
    if (process == (struct process *)0 || process->state != PROCESS_RUNNING)
        return 0;
    current_process = process;
    return 1;
}

int process_exit(struct process *process, int status)
{
    if (process == (struct process *)0 || process->state == PROCESS_EXITED)
        return 0;
    int wake_parent = process->parent != (struct process *)0 &&
        process->parent->state == PROCESS_WAITING;
    process->state = PROCESS_EXITED;
    process->exit_status = status;
    scheduler_terminate(process->main_thread);
    if (process->parent != (struct process *)0) {
        process->parent->state = PROCESS_RUNNING;
        scheduler_unblock(process->parent->main_thread);
        if (wake_parent) {
            vm_activate_root(process_address_space(process->parent));
            scheduler_start_thread(process->parent->main_thread);
        }
    }
    for (unsigned long index = 0; index < process_count; ++index)
        if (process_table[index]->parent == process)
            process_table[index]->parent = (struct process *)0;
    return 1;
}

int process_wait(unsigned long pid, int *status)
{
    if (status == (int *)0) return -1;
    for (unsigned long index = 0; index < process_count; ++index) {
        struct process *process = process_table[index];
        if (process->pid == pid && process->parent == current_process &&
            process->state == PROCESS_EXITED) {
            *status = process->exit_status;
            return (int)pid;
        }
        if (process->pid == pid && process->parent == current_process)
            return -2;
    }
    return -1;
}

int process_wait_blocking(unsigned long pid, int *status)
{
    struct process *waiting_process = current_process;
    for (;;) {
        int result = process_wait(pid, status);
        if (result != -2) return result;
        if (current_process == (struct process *)0) return -1;
        current_process->state = PROCESS_WAITING;
        scheduler_block_current();
        process_set_current(waiting_process);
        current_process->state = PROCESS_RUNNING;
    }
}

int process_kill(unsigned long pid)
{
    for (unsigned long index = 0; index < process_count; ++index) {
        if (process_table[index]->pid == pid)
            return process_exit(process_table[index], -9);
    }
    return 0;
}

int process_load_elf(struct process *process, const void *image,
    unsigned long image_size)
{
    if (process == (struct process *)0) return 0;
    if (!elf_load(process->address_space, image, image_size, &process->entry)) return 0;
    unsigned long physical_stack = memory_alloc_frame();
    if (physical_stack == 0 || !vm_map_page(process->address_space,
        process->stack_top - 0x1000, physical_stack, VM_WRITABLE | VM_USER)) return 0;
    for (unsigned long byte = 0; byte < 0x1000; ++byte)
        ((unsigned char *)physical_stack)[byte] = 0;
    process->image = image;
    process->image_size = image_size;
    return 1;
}

struct process *process_fork(struct process *parent)
{
    if (parent == (struct process *)0 || parent->image == (const void *)0)
        return (struct process *)0;
    struct process *child = process_create((void (*)(void))1);
    if (child == (struct process *)0 || !process_load_elf(child, parent->image,
        parent->image_size)) return (struct process *)0;
    child->parent = parent;
    child->fork_return = 0;
    scheduler_set_entry(child->main_thread, process_user_thread);
    scheduler_set_owner(child->main_thread, child);
    parent->fork_return = (long)child->pid;
    return child;
}

void process_run_user(struct process *process)
{
    if (process == (struct process *)0 || process->state != PROCESS_RUNNING)
        return;
    process_set_current(process);
    scheduler_set_entry(process->main_thread, process_user_thread);
    scheduler_set_owner(process->main_thread, process);
    scheduler_start_thread(process->main_thread);
}

long process_fork_return(const struct process *process)
{
    return process == (const struct process *)0 ? -1 : process->fork_return;
}

struct process *process_take_child(struct process *parent)
{
    if (parent == (struct process *)0) return (struct process *)0;
    for (unsigned long index = 0; index < process_count; ++index) {
        struct process *child = process_table[index];
        if (child->parent == parent && child->state == PROCESS_RUNNING &&
            child->fork_return == 0)
            return child;
    }
    return (struct process *)0;
}

unsigned long process_address_space(const struct process *process)
{
    return process == (const struct process *)0 ? 0 :
        address_space_root(process->address_space);
}

unsigned long process_entry(const struct process *process)
{
    return process == (const struct process *)0 ? 0 : process->entry;
}

unsigned long process_stack_top(const struct process *process)
{
    return process == (const struct process *)0 ? 0 : process->stack_top;
}

unsigned long process_id(const struct process *process)
{
    return process == (const struct process *)0 ? 0 : process->pid;
}

unsigned long process_uid(const struct process *process)
{
    return process == (const struct process *)0 ? 0 : process->uid;
}

unsigned long process_gid(const struct process *process)
{
    return process == (const struct process *)0 ? 0 : process->gid;
}

static void process_test_entry(void)
{
}

int process_self_test(void)
{
    struct process *first = process_create(process_test_entry);
    struct process *second = process_create(process_test_entry);
    int valid = first != (struct process *)0 && second != (struct process *)0 &&
        process_id(first) != 0 && process_id(first) != process_id(second) &&
        process_address_space(first) != 0 &&
        process_address_space(first) != process_address_space(second);
    if (!valid) return 0;
    if (!process_set_current(first)) return 0;
    second->parent = first;
    if (process_fork_return(first) != -1 || process_fork_return(second) != -1)
        return 0;
    if (!process_exit(second, 42)) return 0;
    int status = 0;
    if (!process_set_current(first)) return 0;
    return process_wait(process_id(second), &status) == (int)process_id(second) &&
        status == 42;
}