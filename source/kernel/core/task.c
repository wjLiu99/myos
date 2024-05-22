#include "core/task.h"
#include "tools/kernellib.h"
#include "os_conf.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include"comm/cpu_int.h"

static task_mananger_t task_mananger;
static int tss_init(task_t *task, uint32_t entry, uint32_t esp)
{
    // 为TSS分配GDT
    int tss_sel = gdt_alloc_desc();
    if (tss_sel < 0)
    {
        log_printf("alloc tss failed.\n");
        return -1;
    }
    seg_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t),
                 SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    task->tss_sel = tss_sel;
    kernel_memset(&task->tss, 0, sizeof(tss_t));
    task->tss.eip = entry;
    task->tss.esp = esp; // 未指定栈则用内核栈，即运行在特权级0的进程
    task->tss.esp0 = esp;
    task->tss.ss0 = DS_SELECTOR;
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds = task->tss.fs = task->tss.gs = DS_SELECTOR; // 全部采用同一数据段
    task->tss.cs = CS_SELECTOR;
    task->tss.iomap = 0;
    return 0;
}

int task_init (task_t *task, const char * name, uint32_t entry, uint32_t esp)
{

    ASSERT(task != (task_t *)0);
    tss_init(task, entry, esp);
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    // uint32_t *pesp =  (uint32_t *)esp;
    // if(pesp){
    //     *(--pesp) = entry;
    //     *(--pesp)=0;
    //     *(--pesp)=0;
    //     *(--pesp)=0;
    //     *(--pesp)=0;
    //     task->stack = pesp;
    // }
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    tast_set_ready(task);
    list_insert_last(&task_mananger.task_list,&task->all_node);
    return 0;
}
void simple_switch(uint32_t **from, uint32_t *to);

void task_switch_from_to(task_t *from, task_t *to)
{
    switch_to_tss(to->tss_sel);
    // simple_switch(&from->stack, to->stack);
}

void task_mananger_init(void){
    list_init(&task_mananger.task_list);
    list_init(&task_mananger.ready_list);
    task_mananger.curr_task=(task_t *)0;
    

}

void task_first_init(void){
    task_init(&task_mananger.first_task, "first task",(uint32_t)0, (uint32_t)0); // 正在运行的进程不用设置入口地址
    write_tr(task_mananger.first_task.tss_sel);
    task_mananger.curr_task=&task_mananger.first_task;
}

task_t *task_first_task(void){
    return &task_mananger.first_task;
}
void tast_set_ready(task_t  *task){
    list_insert_last(&task_mananger.ready_list,&task->run_node);
    task->state = TASK_READY;
}
void task_set_block(task_t *task){
    list_remove(&task_mananger.ready_list,&task->run_node);
    
}
static task_t * task_next_run (void) {

    // 普通任务
    list_node_t * task_node = list_first(&task_mananger.ready_list);
    return (task_t *)list_node_parent(task_node, task_t, run_node);
}
int sys_sched_yield(void){
    if (list_count(&task_mananger.ready_list) > 1) {
        task_t * curr_task = task_current();

        
        task_set_block(curr_task);
        task_set_ready(curr_task);

        task_dispatch();
    }
}
void task_dispatch (void) {
    task_t * to = task_next_run();
    if (to != task_mananger.curr_task) {
        task_t * from = task_mananger.curr_task;

        task_mananger.curr_task = to;
        task_switch_from_to(from, to);
    }
}
