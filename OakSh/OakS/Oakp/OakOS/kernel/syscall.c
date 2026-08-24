#include "acorn/syscall.h"
#include "acorn/keyboard.h"
#include "acorn/scheduler.h"
#include "acorn/serial.h"
#include "acorn/process.h"
#include "acorn/vm.h"
#include "acorn/fs.h"
#include "acorn/socket.h"
#include "acorn/framebuffer.h"

extern void acorn_enter_user(void (*entry)(void), unsigned long stack_top);

static long syscall_write(unsigned long text, unsigned long length)
{
    if (text == 0 || length == 0 || length > 4096) return -1;
    for (unsigned long index = 0; index < length; ++index) {
        char character[2] = { ((const char *)text)[index], '\0' };
        serial_write(character);
    }
    return (long)length;
}

long syscall_dispatch(unsigned long number, unsigned long arg0,
    unsigned long arg1, unsigned long arg2)
{
    (void)arg2;
    switch (number) {
    case SYSCALL_WRITE:
        return syscall_write(arg0, arg1);
    case SYSCALL_GETPID:
        return process_id(process_current());
    case SYSCALL_YIELD:
        scheduler_yield();
        return 0;
    case SYSCALL_EXIT:
        return process_current() != (struct process *)0 ? 0 : -1;
    case SYSCALL_WAITPID:
        return process_wait(arg0, (int *)arg1);
    case SYSCALL_KILL:
        return process_kill(arg0) ? 0 : -1;
    case SYSCALL_FORK: {
        if (process_current() != (struct process *)0 &&
            process_fork_return(process_current()) == 0)
            return 0;
        struct process *child = process_fork(process_current());
        return child == (struct process *)0 ? -1 : (long)process_id(child);
    }
    case SYSCALL_MKDIR:
        return fs_mkdir((const char *)arg0);
    case SYSCALL_CREATE:
        return fs_create((const char *)arg0);
    case SYSCALL_WRITE_FILE:
        return fs_write((const char *)arg0, (const void *)arg1, arg2);
    case SYSCALL_READ_FILE:
        return fs_read((const char *)arg0, (void *)arg1, arg2);
    case SYSCALL_GETUID:
        return (long)process_uid(process_current());
    case SYSCALL_GETGID:
        return (long)process_gid(process_current());
    case SYSCALL_SOCKET:
        return socket_open((int)arg0, (int)arg1, (int)arg2);
    case SYSCALL_CONNECT:
        return socket_connect((int)arg0, (unsigned int)arg1,
            (unsigned short)arg2);
    case SYSCALL_SEND:
        return socket_send((int)arg0, (const void *)arg1, arg2);
    case SYSCALL_RECV:
        return socket_receive((int)arg0, (void *)arg1, arg2);
    case SYSCALL_CLOSE:
        return socket_close((int)arg0);
    case SYSCALL_FB_CLEAR:
        framebuffer_clear((unsigned int)arg0);
        return 0;
    case SYSCALL_FB_TEXT:
        framebuffer_draw_text((unsigned int)arg0, (unsigned int)arg1,
            (const char *)arg2, 0xD8F3DC, 1);
        return 0;
    default:
        return -1;
    }
}

long acorn_syscall_dispatch_entry(unsigned long number, unsigned long arg0,
    unsigned long arg1, unsigned long arg2)
{
    return syscall_dispatch(number, arg0, arg1, arg2);
}

void acorn_user_exit(void)
{
    struct process *parent = process_current();
    struct process *child = process_take_child(parent);
    if (child != (struct process *)0) {
        process_exit(parent, 0);
        process_run_user(child);
    }
    process_exit(parent, 0);
    serial_write("user process exited\n");
    for (;;) __asm__ volatile ("sti; hlt");
}

int syscall_self_test(void)
{
    static const char message[] = "syscall write: OK\n";
    struct process *process = process_create((void (*)(void))1);
    if (process == (struct process *)0 || !process_set_current(process)) return 0;
    long pid = syscall_dispatch(SYSCALL_GETPID, 0, 0, 0);
    return pid > 0 && syscall_dispatch(99, 0, 0, 0) == -1 &&
        syscall_dispatch(SYSCALL_WRITE, (unsigned long)message,
            sizeof(message) - 1, 0) == (long)(sizeof(message) - 1);
}