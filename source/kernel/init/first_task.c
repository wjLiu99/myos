#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"
int first_task_main(void)
{
    int count = 3;
    int pid = getpid();
    pid = fork();
    if (pid < 0)
    {
        print_msg("create child failed.\n", 0);
    }
    else if (pid == 0)
    {
        print_msg("im child : %d", getpid());
        char *argv[] = {"arg0", "arg1"};
        execve("/shell.elf", argv, (char **)0);
    }
    else
    {
        print_msg("child : %d", pid);
        print_msg("parent : %d", count);
    }
    while (1)
    {
        print_msg("first task id = %d.", pid);
        msleep(1000);
    }
    return 0;
}