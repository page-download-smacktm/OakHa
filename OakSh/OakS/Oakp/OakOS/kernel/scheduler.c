#include "acorn/scheduler.h"
#include "acorn/memory.h"

enum { MAX_THREADS = 32, THREAD_STACK_SIZE = 16 * 1024 };

struct context {
    unsigned long rsp;
    unsigned long rbp;
    unsigned long rbx;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
};

struct thread {
    unsigned long id;
    enum thread_state state;
    thread_entry entry;
    struct context context;
    unsigned long stack_bottom;
    unsigned long stack_top;
    void *owner;
    int started;
};

extern void acorn_switch_context(struct context *old, struct context *next);

static struct thread *threads[MAX_THREADS];
static unsigned long thread_count;
static unsigned long next_id;
static unsigned long current_index;
static struct context scheduler_context;
static int scheduler_active;

static void thread_bootstrap(void)
{
    struct thread *thread = scheduler_current();
    thread->started = 1;
    thread->entry();
    thread->state = THREAD_TERMINATED;
    scheduler_yield();
    for (;;) __asm__ volatile ("hlt");
}

void scheduler_init(void)
{
    for (unsigned long index = 0; index < MAX_THREADS; ++index)
        threads[index] = (struct thread *)0;
    thread_count = 0;
    next_id = 1;
    current_index = 0;
    scheduler_active = 0;
}

struct thread *thread_create(thread_entry entry)
{
    if (entry == (thread_entry)0 || thread_count == MAX_THREADS)
        return (struct thread *)0;
    struct thread *thread = (struct thread *)kmalloc(sizeof(struct thread));
    if (thread == (struct thread *)0) return (struct thread *)0;
    thread->id = next_id++;
    thread->state = THREAD_READY;
    thread->entry = entry;
    thread->stack_bottom = (unsigned long)kmalloc(THREAD_STACK_SIZE);
    if (thread->stack_bottom == 0) return (struct thread *)0;
    thread->stack_top = thread->stack_bottom + THREAD_STACK_SIZE;
    thread->owner = (void *)0;
    thread->started = 0;
    thread->stack_top &= ~15UL;
    thread->stack_top -= 8;
    *(unsigned long *)thread->stack_top = (unsigned long)thread_bootstrap;
    thread->context.rsp = thread->stack_top;
    threads[thread_count++] = thread;
    return thread;
}

void scheduler_set_entry(struct thread *thread, thread_entry entry)
{
    if (thread != (struct thread *)0 && entry != (thread_entry)0)
        thread->entry = entry;
}

void scheduler_set_owner(struct thread *thread, void *owner)
{
    if (thread != (struct thread *)0) thread->owner = owner;
}

void scheduler_terminate(struct thread *thread)
{
    if (thread != (struct thread *)0)
        thread->state = THREAD_TERMINATED;
}

void scheduler_block_current(void)
{
    if (thread_count == 0 || !scheduler_active) return;
    threads[current_index]->state = THREAD_BLOCKED;
    scheduler_yield();
}

void scheduler_unblock(struct thread *thread)
{
    if (thread != (struct thread *)0 && thread->state == THREAD_BLOCKED)
        thread->state = THREAD_READY;
}

void *scheduler_current_owner(void)
{
    struct thread *thread = scheduler_current();
    return thread == (struct thread *)0 ? (void *)0 : thread->owner;
}

struct thread *scheduler_current(void)
{
    if (thread_count == 0) return (struct thread *)0;
    return threads[current_index];
}

void scheduler_tick(void)
{
    if (thread_count == 0) return;
    if (!scheduler_active) {
        threads[current_index]->state = THREAD_READY;
        for (unsigned long offset = 1; offset <= thread_count; ++offset) {
            unsigned long candidate = (current_index + offset) % thread_count;
            if (threads[candidate]->state == THREAD_READY) {
                current_index = candidate;
                threads[current_index]->state = THREAD_RUNNING;
                return;
            }
        }
        return;
    }
    scheduler_yield();
}

void scheduler_preempt(void)
{
    if (!scheduler_active || thread_count < 2) return;
    for (unsigned long offset = 1; offset < thread_count; ++offset) {
        unsigned long candidate = (current_index + offset) % thread_count;
        if (threads[candidate]->state == THREAD_READY && threads[candidate]->started) {
            scheduler_yield();
            return;
        }
    }
}

void scheduler_yield(void)
{
    if (thread_count == 0 || !scheduler_active) return;
    struct thread *previous = threads[current_index];
    if (previous->state == THREAD_RUNNING)
        previous->state = THREAD_READY;
    for (unsigned long offset = 1; offset <= thread_count; ++offset) {
        unsigned long candidate = (current_index + offset) % thread_count;
        if (threads[candidate]->state == THREAD_READY) {
            current_index = candidate;
            threads[current_index]->state = THREAD_RUNNING;
            acorn_switch_context(&previous->context, &threads[current_index]->context);
            return;
        }
    }
    previous->state = THREAD_TERMINATED;
    scheduler_active = 0;
    acorn_switch_context(&previous->context, &scheduler_context);
}

void scheduler_start(void)
{
    if (thread_count == 0) return;
    current_index = 0;
    threads[current_index]->state = THREAD_RUNNING;
    scheduler_active = 1;
    acorn_switch_context(&scheduler_context, &threads[current_index]->context);
}

void scheduler_start_thread(struct thread *thread)
{
    if (thread == (struct thread *)0) return;
    for (unsigned long index = 0; index < thread_count; ++index) {
        if (threads[index] == thread) {
            current_index = index;
            thread->state = THREAD_RUNNING;
            scheduler_active = 1;
            acorn_switch_context(&scheduler_context, &thread->context);
            return;
        }
    }
}

static void scheduler_test_entry(void)
{
}

int scheduler_self_test(void)
{
    scheduler_init();
    struct thread *first = thread_create(scheduler_test_entry);
    struct thread *second = thread_create(scheduler_test_entry);
    struct thread *third = thread_create(scheduler_test_entry);
    if (first == (struct thread *)0 || second == (struct thread *)0 ||
        third == (struct thread *)0) return 0;
    first->state = THREAD_RUNNING;
    if (scheduler_current() != first) return 0;
    scheduler_tick();
    if (scheduler_current() != second) return 0;
    scheduler_tick();
    return scheduler_current() == third;
}