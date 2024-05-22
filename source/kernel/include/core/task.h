#ifndef TASK_H
#define TASK_H

#include"cpu/cpu.h"
#include"comm/types.h"
#include"tools/list.h"

#define TASK_NAME_SIZE				32			// 任务名字长度
//进程控制块
typedef struct _task_t
{
    //任务状态
     enum {
		TASK_CREATED,
		TASK_RUNNING,
		TASK_SLEEP,
		TASK_READY,
		TASK_WAITING,
		TASK_ZOMBIE,
	}state;
    list_node_t *run_node;
    list_node_t *all_node;
    char name[TASK_NAME_SIZE];		// 任务名字
    //uint32_t *stack;   //任务切换时保存栈顶指针,用于自定义切换
    tss_t tss;
    uint16_t tss_sel;		// tss选择子
}task_t;

int task_init (task_t *task, const char * name,  uint32_t entry, uint32_t esp);
void task_switch_from_to (task_t * from, task_t * to);
void task_set_ready(task_t *task);
void task_set_block (task_t *task);
//任务管理器
typedef struct _task_mananger_t{
    task_t *curr_task;
    list_t ready_list;
    list_t task_list;
    task_t first_task;
}task_mananger_t;
void task_mananger_init(void);
void task_first_init(void);
task_t *task_first_task(void);
void tast_set_ready(task_t  *task);
void task_set_block(task_t *task);
int sys_sched_yield(void);
void task_dispatch (void);
#endif