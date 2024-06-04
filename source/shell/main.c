#include "lib_syscall.h"
#include <stdio.h>
#include <string.h>
// #include <stdlib.h>
#include "main.h"
static cli_t cli;
static const char *promot = "sh >>";
char cmd_buf[256];
static int do_help(int argc, char **argv)
{
    const cli_cmd_t *start = cli.cmd_start;
    while (start < cli.cmd_end)
    {
        printf("%s %s\n", start->name, start->usage);
        start++;
    }
    return 0;
}
static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
        .usage = "help --list supported command",
        .do_func = do_help,
    },
};

static void cli_init(const char *promot, const cli_cmd_t *cmd_list, int size)
{
    cli.promot = promot;
    memset(cli.curr_input, 0, CLI_INPUT_SIZE);
    cli.cmd_start = cmd_list;
    cli.cmd_end = cmd_list + size;
}

static void show_promot(void)
{
    printf("%s", cli.promot);
    fflush(stdout);
}

static const cli_cmd_t *find_buildin(const char *name)
{
    for (const cli_cmd_t *cmd = cli.cmd_start; cmd < cli.cmd_end; cmd++)
    {
        if (strcmp(cmd->name, name) != 0)
        {
            continue;
        }

        return cmd;
    }

    return (const cli_cmd_t *)0;
}

static void run_buildin(const cli_cmd_t *cmd, int argc, char **argv)
{
    int ret = cmd->do_func(argc, argv);
    if (ret < 0)
    {
        fprintf(stderr, "error:%d\n", ret);
    }
}
int main(int argc, char **argv)
{
#if 0
    sbrk(0);
    sbrk(100);
    sbrk(200);
    sbrk(4096 * 2 + 200);

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
#endif
    open(argv[0], 0);
    dup(0);
    dup(0);
    printf("hello from shell.\n");
    printf("os version : %s\n", OS_VERSION);

    cli_init(promot, cmd_list, sizeof(cmd_list) / sizeof(cmd_list[0]));

    for (;;)
    {
        show_promot();
        char *str = fgets(cli.curr_input, CLI_INPUT_SIZE, stdin);
        if (!str)
        {
            continue;
        }
        char *cr = strchr(cli.curr_input, '\n');
        if (cr)
        {
            *cr = '\0';
        }
        cr = strchr(cli.curr_input, '\r');
        if (cr)
        {
            *cr = '\0';
        }
        int argc = 0;
        char *argv[CLI_MAX_ARG_COUNT];
        memset(argv, 0, sizeof(argv));
        const char *space = " ";
        char *token = strtok(cli.curr_input, space);
        while (token)
        {
            argv[argc++] = token;
            token = strtok(NULL, space);
        }
        if (argc == 0)
        {
            continue;
        }
        const cli_cmd_t *cmd = find_buildin(argv[0]);
        if (cmd)
        {
            run_buildin(cmd, argc, argv);
            continue;
        }
    }
}