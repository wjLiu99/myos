#include "lib_syscall.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    sbrk(0);
    sbrk(100);
    sbrk(200);
    sbrk(4096 * 2 + 200);
    printf("hello from shell.\n");
    printf("os version : %s\n", OS_VERSION);
    printf("abef\b\b\b\bcd\n");
    printf("abcd\x7f;g\n");
    printf("\0337Hello,world!\0338123\n");
    printf("\033[31;42mHELLO,WORLD!\033[39;49m123\n");

    printf("123\033[2Dhello,world!\n");
    printf("123\033[2Chello,world!\n");
    printf("\033[31m");
    printf("\033[10;10H test!\n");
    printf("\033[20;20H test!\n]");
    printf("\033[32;15;39m123\n");
    printf("\033[2J\n");

    for (int i = 0; i < argc; i++)
    {
        printf("arg: %s\n", argv[i]);
    }
    for (;;)
    {
        // printf("hello\n");
        msleep(1000);
    }
}