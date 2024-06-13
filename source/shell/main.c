#include "lib_syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include <getopt.h>
#include <sys/file.h>
#include "fs/file.h"
#include "dev/tty.h"
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
static int do_clear(int argc, char **argv)
{
    printf("%s", ESC_CLEAR_SCREEN);
    printf("%s", ESC_MOVE_CURSOR(0, 0));
    return 0;
}
static int do_echo(int argc, char **argv)
{
    if (argc == 1)
    {
        char msg_buf[128];
        fgets(msg_buf, sizeof(msg_buf), stdin);
        msg_buf[sizeof(msg_buf) - 1] = '\n';
        puts(msg_buf);
        return 0;
    }

    int count = 1;
    int ch;
    optind = 1;
    while ((ch = getopt(argc, argv, "n:h")) != -1)
    {
        switch (ch)
        {
        case 'h':
            puts("echo any message\n");
            return 0;
        case 'n':
            count = atoi(optarg);
            break;
        case '?':

            if (optarg)
            {
                fprintf(stderr, "unknown option %s\n", optarg);
            }

            return -1;

        default:
            break;
        }
    }
    if (optind > argc - 1)
    {
        fprintf(stderr, "message is empty\n");

        return -1;
    }
    char *msg = argv[optind];
    for (int i = 0; i < count; i++)
    {
        puts(msg);
    }

    return 0;
}
static int do_exit(int argc, char **argv)
{
    exit(0);
    return 0;
}
static int do_ls(int argc, char **argv)
{
    DIR *p_dir = opendir("temp");
    if (p_dir == NULL)
    {
        printf("opendir failed.");
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(p_dir)) != NULL)
    {
        strlwr(entry->name);
        printf("%c %s %d\n",
               entry->type == FILE_DIR ? 'd' : 'f', entry->name, entry->size);
    }
    closedir(p_dir);
    return 0;
}
static int do_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "no file");
        return -1;
    }
    int err = unlink(argv[1]);
    if (err < 0)
    {
        fprintf(stderr, "rm file failed.%s", argv[1]);
        return err;
    }
    return 0;
}
static int do_cp(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, " no from or to");
        return -1;
    }

    FILE *from, *to;
    from = fopen(argv[1], "rb");
    to = fopen(argv[2], "wb");
    if (!from || !to)
    {
        fprintf(stderr, "open file failed");
        goto cp_failed;
    }
    char *buf = (char *)malloc(255);
    memset(buf, 0, sizeof(buf));
    int size = 0;
    while ((size = fread(buf, 1, 255, from)) > 0)
    {
        // fwrite(buf, 1, 255, to);不对
        fwrite(buf, 1, size, to);
    }
    free(buf);

cp_failed:
    if (from)
    {
        fclose(from);
    }
    if (to)
    {
        fclose(to);
    }
    return 0;
}
static int do_less(int argc, char **argv)
{
    int line_mode = 0;

    int ch;
    optind = 1;
    while ((ch = getopt(argc, argv, "lh")) != -1)
    {
        switch (ch)
        {
        case 'h':
            puts("show file content\n");
            optind = 1;
            return 0;

        case 'l':
            line_mode = 1;
            // optind = 1;

            break;
        case '?':

            if (optarg)
            {
                fprintf(stderr, "unknown option %s\n", optarg);
            }
            optind = 1;

            return -1;

        default:
            break;
        }
    }
    if (optind > argc - 1)
    {
        fprintf(stderr, "no file\n");
        optind = 1;
        return -1;
    }

    FILE *file = fopen(argv[optind], "r");
    if (file == NULL)
    {
        fprintf(stderr, "open file failed.%s", argv[optind]);
        optind = 1;
        return -1;
    }
    char *buf = (char *)malloc(255);
    memset(buf, 0, sizeof(buf));
    if (line_mode == 0)
    {

        while (fgets(buf, 255, file) != NULL)
        {
            fputs(buf, stdout);
        }
    }
    else
    {
        ioctl(0, TTY_CMD_ECHO, 0, 0);
        setvbuf(stdin, NULL, _IONBF, 0);
        while (1)
        {
            char *b = fgets(buf, 255, file);
            if (b == NULL)
            {
                break;
            }
            fputs(buf, stdout);
            int c;
            while ((c = fgetc(stdin)) != 'n')
            {
                if (c == 'q')
                {
                    goto less_quit;
                }
            }
        }
    less_quit:
        setvbuf(stdin, NULL, _IOLBF, BUFSIZ);
        ioctl(0, TTY_CMD_ECHO, 1, 0);
    }
    free(buf);
    fclose(file);
    return 0;
}
static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
        .usage = "help --list supported command",
        .do_func = do_help,
    },
    {
        .name = "clear",
        .usage = "clear  --claer screen",
        .do_func = do_clear,
    },
    {
        .name = "echo",
        .usage = "echo [-n count] msg",
        .do_func = do_echo,
    },
    {
        .name = "quit",
        .usage = "quit from shell",
        .do_func = do_exit,
    },
    {
        .name = "ls",
        .usage = "ls --list director",
        .do_func = do_ls,
    },
    {
        .name = "less",
        .usage = "less [-l] file -- show file",
        .do_func = do_less,
    },
    {
        .name = "cp",
        .usage = "cp src dest",
        .do_func = do_cp,
    },
    {
        .name = "rm",
        .usage = "remove src",
        .do_func = do_rm,
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
static const char *find_exec_path(const char *filename)
{
    static char path[255];
    int fd = open(filename, 0);
    if (fd < 0)
    {
        sprintf(path, "%s.elf", filename);
        fd = open(path, 0);
        if (fd < 0)
        {
            return (const char *)0;
        }
        close(fd);
        return path;
    }
    else
    {
        close(fd);
        return filename;
    }
}
static void run_exec_file(const char *path, int argc, char **argv)
{
    int pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork failed %s", path);
    }
    else if (pid == 0)
    {
        int err = execve(path, argv, (char *const *)0);
        if (err < 0)
        {
            fprintf(stderr, "exec failed :%s ", path);
        }
    }
    else
    {
        int status;
        int pid = wait(&status);
        fprintf(stderr, "cmd %s result :%d,pid = %d\n", path, status, pid);
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
    open(argv[0], O_RDWR);
    dup(0);
    dup(0);
    printf("hello from shell.\n");
    printf("os version : %s\n", OS_VERSION);

    cli_init(promot, cmd_list, sizeof(cmd_list) / sizeof(cmd_list[0]));

    for (;;)
    {
        optind = 1;

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

        const char *path = find_exec_path(argv[0]);
        if (path)
        {
            run_exec_file(path, argc, argv);
            continue;
        }

        fprintf(stderr, ESC_COLOR_ERROR "Unknown command :%s\n" ESC_COLOR_DEFAULT, cli.curr_input);
    }
}