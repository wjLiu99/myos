#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H
#include "core/syscall.h"
#include "os_conf.h"
#include <sys/stat.h>

typedef struct _syscall_args_t
{
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;

} syscall_args_t;

void msleep(int ms);
int getpid(void);

void print_msg(const char *fmt, int arg);

int fork(void);

int execve(const char *name, char *const *argv, char *const *env);
int yield(const char *name, char *const *argv, char *const *env);

int open(const char *name, int flags, ...);
int read(int file, char *ptr, int len);
int write(int file, char *ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);

int isatty(int file);
int fstat(int file, struct stat *st);
void *sbrk(ptrdiff_t incr);
void _exit(int status);
int wait(int *status);
int dup(int file);
#endif