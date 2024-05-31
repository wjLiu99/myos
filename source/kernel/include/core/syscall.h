#ifndef SYSCALL_H
#define SYSCALL_H
#include "comm/types.h"

#define SYSCALL_PARAM_COUNT 5
#define SYS_sleep 0
#define SYS_getpid 1
#define SYS_print_msg 2
#define SYS_fork 3
#define SYS_execve 4
#define SYS_yield 5

#define SYS_open 40
#define SYS_read 41
#define SYS_write 42
#define SYS_close 43
#define SYS_lseek 44
#define SYS_isatty 45
#define SYS_fstat 46
#define SYS_sbrk 47

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