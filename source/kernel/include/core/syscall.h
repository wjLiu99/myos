#ifndef SYSCALL_H
#define SYSCALL_H
#include "comm/types.h"

#define SYSCALL_PARAM_COUNT 5
#define SYS_sleep 0
#define SYS_getpid 1
#define SYS_print_msg 2
#define SYS_fork 3
#define SYS_execve 4

typedef struct _syscall_frame_t
{
    int eflags;
    int gs, fs, es, ds;
    uint32_t edi, esi, ebp, dummy, ebx, edx, ecx, eax;
    int eip, cs;
    int func_id, arg0, arg1, arg2, arg3;
    int esp, ss;

} syscall_frame_t;
void exception_handler_syscall(void);

#endif