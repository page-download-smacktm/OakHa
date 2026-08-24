#ifndef ACORN_SCHEDULER_H
#define ACORN_SCHEDULER_H

enum thread_state {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED,
};

struct thread;
typedef void (*thread_entry)(void);

void scheduler_init(void);
struct thread *thread_create(thread_entry entry);
struct thread *scheduler_current(void);
void scheduler_tick(void);
void scheduler_preempt(void);
void scheduler_yield(void);
void scheduler_start(void);
void scheduler_start_thread(struct thread *thread);
void scheduler_set_entry(struct thread *thread, thread_entry entry);
void scheduler_set_owner(struct thread *thread, void *owner);
void scheduler_terminate(struct thread *thread);
void scheduler_block_current(void);
void scheduler_unblock(struct thread *thread);
void *scheduler_current_owner(void);
int scheduler_self_test(void);

#endif