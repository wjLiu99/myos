#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H
#include "core/syscall.h"
#include "comm/types.h"
#include "os_conf.h"
#include "core/syscall.h"

typedef struct _syscall_args_t
{
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;

} syscall_args_t;

static inline int sys_call(syscall_args_t *args)
{
    int ret;
    // 权限0
    uint32_t addr[] = {0, SYSCALL_SELECTOR | 0};
    __asm__ __volatile__(
        "push %[arg3]\n\t"
        "push %[arg2]\n\t"
        "push %[arg1]\n\t"
        "push %[arg0]\n\t"
        "push %[id]\n\t"
        "lcalll *(%[a])" : "=a"(ret) :
        [arg3] "r"(args->arg3),
        [arg2] "r"(args->arg2),
        [arg1] "r"(args->arg1),
        [arg0] "r"(args->arg0),
        [id] "r"(args->id),
        [a] "r"(addr));
    return ret;
}

static inline void msleep(int ms)
{
    if (ms <= 0)
    {
        return;
    }
    syscall_args_t args;
    args.id = SYS_sleep;
    args.arg0 = ms;

    sys_call(&args);
}

static inline int getpid(void)
{
    syscall_args_t args;
    args.id = SYS_getpid;

    return sys_call(&args);
}

static inline void print_msg(const char *fmt, int arg)
{
    syscall_args_t args;
    args.id = SYS_print_msg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;

    sys_call(&args);
}

static inline int fork(void)
{
    syscall_args_t args;
    args.id = SYS_fork;
    sys_call(&args);
}
static inline int execve(const char *name, char *const *argv, char *const *env)
{
    syscall_args_t args;
    args.id = SYS_execve;
    args.arg0 = (int)name;
    args.arg1 = (int)argv;
    args.arg2 = (int)env;

    return sys_call(&args);
}
static inline int yield(const char *name, char *const *argv, char *const *env)
{
    syscall_args_t args;
    args.id = SYS_yield;

    return sys_call(&args);
}
#endif