#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "comm/types.h"
#include "tools/list.h"
#include "fs/file.h"

#define TASK_NAME_SIZE 32 // 任务名字长度
#define TASK_TIME_SLICE_DEFAULT 10
#define TASK_FLAGS_SYSTEM (1 << 0)
#define TASK_OFILE_NR 128
// 进程控制块
typedef struct _task_t
{
    // 任务状态
    enum
    {
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP,
        TASK_READY,
        TASK_WAITING,
        TASK_ZOMBIE,
    } state;
    int pid;
    file_t *file_table[TASK_OFILE_NR];
    int status;
    uint32_t heap_start;
    uint32_t heap_end;
    struct _task_t *parent;
    int sleep_ticks; // 延时节拍数
    int time_ticks;
    int slice_ticks; // 进程能运行的最大时钟节拍次数
    list_node_t run_node;
    list_node_t all_node;
    list_node_t wait_node;
    char name[TASK_NAME_SIZE]; // 任务名字
    // uint32_t *stack;   //任务切换时保存栈顶指针,用于自定义切换
    tss_t tss;
    uint16_t tss_sel; // tss选择子
} task_t;

int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);

// 任务管理器
typedef struct _task_manager_t
{
    task_t *curr_task;
    list_t ready_list;
    list_t task_list;
    list_t sleep_list;
    task_t first_task;
    task_t idle_task;
    int app_code_sel; // 给所以用户程序使用的段选择子
    int app_data_sel;
} task_manager_t;

typedef struct _task_args_t
{
    uint32_t ret_addr;
    uint32_t argc;
    char **argv;
} task_args_t;
void task_remove_fd(int fd);
file_t *task_file(int fd);
int task_alloc_fd(file_t *file);

void task_manager_init(void);
void task_first_init(void);
task_t *task_first_task(void);
void tast_set_ready(task_t *task);
void task_set_block(task_t *task);
void task_set_ready(task_t *task);
void task_set_block(task_t *task);
int sys_yield(void);
task_t *task_current(void);
void task_dispatch(void);
void sys_sleep(uint32_t ms);
void task_set_sleep(task_t *task, uint32_t ticks);
void task_set_wakeup(task_t *task);
void task_time_tick();
int sys_getpid(void);
int sys_fork(void);
int sys_execve(char *name, char **argv, char **env);

void sys_exit(int status);
int sys_wait(int *status);

static task_t *alloc_task(void);

static void free_task(task_t *task);
#endif