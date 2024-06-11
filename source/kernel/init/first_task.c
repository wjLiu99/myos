#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"
#include "dev/tty.h"
int first_task_main(void)
{
#if 0
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
#endif
    for (int i = 0; i < 1; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            print_msg("creat shell failed", 0);
            break;
        }
        else if (pid == 0)
        {
            char tty_num[] = "/dev/tty?";
            tty_num[sizeof(tty_num) - 2] = i + '0';
            char *argv[] = {tty_num, (char *)0};
            execve("/shell.elf", argv, (char **)0);
            while (1)
            {
                msleep(1000);
            }
        }
    }
    while (1)
    {
        // print_msg("first task id = %d.", pid);
        msleep(1000);
    }
    return 0;
}