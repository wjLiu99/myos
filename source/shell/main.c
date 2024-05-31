#include "lib_syscall.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    sbrk(0);
    sbrk(100);
    sbrk(200);
    sbrk(4096 * 2 + 200);
    printf("hello from shell.\n");
    for (int i = 0; i < argc; i++)
    {
        print_msg("arg: %s", argv[i]);
    }
    for (;;)
    {
        msleep(1000);
    }
}