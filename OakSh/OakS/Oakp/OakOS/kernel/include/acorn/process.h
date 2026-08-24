#ifndef ACORN_PROCESS_H
#define ACORN_PROCESS_H

struct process;

enum process_state {
	PROCESS_RUNNING,
	PROCESS_WAITING,
	PROCESS_EXITED,
};

struct process *process_create(void (*entry)(void));
int process_load_elf(struct process *process, const void *image,
	unsigned long image_size);
unsigned long process_id(const struct process *process);
unsigned long process_uid(const struct process *process);
unsigned long process_gid(const struct process *process);
unsigned long process_address_space(const struct process *process);
unsigned long process_entry(const struct process *process);
unsigned long process_stack_top(const struct process *process);
struct process *process_current(void);
int process_exit(struct process *process, int status);
int process_wait(unsigned long pid, int *status);
int process_wait_blocking(unsigned long pid, int *status);
int process_kill(unsigned long pid);
int process_set_current(struct process *process);
struct process *process_fork(struct process *parent);
long process_fork_return(const struct process *process);
struct process *process_take_child(struct process *parent);
void process_run_user(struct process *process);
int elf_loader_self_test(void);
int process_self_test(void);

#endif