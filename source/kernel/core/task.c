#include "core/task.h"
#include "tools/kernellib.h"
#include "os_conf.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_int.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/mmu.h"

static task_manager_t task_manager;
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

    int code_sel, data_sel;
    code_sel = task_manager.app_code_sel | SEG_RPL3;
    data_sel = task_manager.app_data_sel | SEG_RPL3;
    task->tss.eip = entry;
    task->tss.esp = esp; // 未指定栈则用内核栈，即运行在特权级0的进程
    task->tss.esp0 = esp;
    task->tss.ss0 = DS_SELECTOR; //特权0使用的栈
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds = task->tss.fs = task->tss.gs = data_sel; // 全部采用同一数据段
    task->tss.cs = code_sel;
    task->tss.iomap = 0;
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0)
    {
        gdt_free_sel(tss_sel);
        return -1;
    }
    task->tss.cr3 = page_dir;
    return 0;
}

int task_init(task_t *task, const char *name, uint32_t entry, uint32_t esp)
{

    ASSERT(task != (task_t *)0);
    tss_init(task, entry, esp);
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->time_ticks = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_ticks;
    task->sleep_ticks = 0;
    // 栈是先减再压数据
    //  uint32_t *pesp =  (uint32_t *)esp;
    //  if(pesp){
    //      *(--pesp) = entry;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      task->stack = pesp;
    //  }
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}
void simple_switch(uint32_t **from, uint32_t *to);

void task_switch_from_to(task_t *from, task_t *to)
{
    switch_to_tss(to->tss_sel);
    // simple_switch(&from->stack, to->stack);
}

static void idle_task_entry(void)
{
    while (1)
    {
        hlt();
    }
}
static uint32_t idle_task_stack[1024];
void task_manager_init(void)
{
    int sel = gdt_alloc_desc();
    seg_desc_set(sel, 0x00000000, 0xffffffff, SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_data_sel = sel;
    sel = gdt_alloc_desc();
    seg_desc_set(sel, 0x00000000, 0xffffffff, SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = sel;
    list_init(&task_manager.task_list);
    list_init(&task_manager.ready_list);
    list_init(&task_manager.sleep_list);
    task_manager.curr_task = (task_t *)0;
    task_init(&task_manager.idle_task,
              "idle task",
              (uint32_t)idle_task_entry,
              (uint32_t)(&idle_task_stack[1024]));
}

void task_first_init(void)
{
    void first_task_entry(void);
    extern uint8_t s_first_task[], e_first_task[];

    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
    ASSERT(copy_size <= alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry;
    task_init(&task_manager.first_task, "first task", first_start, (uint32_t)0); // 正在运行的进程不用设置入口地址
    write_tr(task_manager.first_task.tss_sel);
    task_manager.curr_task = &task_manager.first_task;

    // 现在cr3已经是进程自己的页表了，tss_init的时候给进程分配了新的页表
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // s_first_task是物理地址，但是页表中mem_ext_end以下的虚拟地址和物理地址完成了一一映射
    // fisrt_start是虚拟地址，在分配完新的页表项后以及完成映射，可以直接写
    memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W);
    kernel_memcpy((void *)first_start, s_first_task, copy_size);
}

task_t *task_first_task(void)
{
    return &task_manager.first_task;
}

// 将任务从就绪队列移除
void task_set_block(task_t *task)
{
    if (task == &task_manager.idle_task)
    {
        return;
    }
    list_remove(&task_manager.ready_list, &task->run_node);
}

task_t *task_current(void)
{
    return task_manager.curr_task;
}

static task_t *task_next_run(void)
{

    if (list_count(&task_manager.ready_list) == 0)
    {
        return &task_manager.idle_task;
    }
    // 普通任务
    list_node_t *task_node = list_first(&task_manager.ready_list);
    return (task_t *)list_node_parent(task_node, task_t, run_node);
}

void task_set_ready(task_t *task)
{
    if (task == &task_manager.idle_task)
    {
        return;
    }
    list_insert_last(&task_manager.ready_list, &task->run_node);
    task->state = TASK_READY;
}

int sys_sched_yield(void)
{
    irq_state_t state = irq_enter_protection();
    if (list_count(&task_manager.ready_list) > 1)
    {
        task_t *curr_task = task_current();
        task_set_block(curr_task);
        task_set_ready(curr_task);
        task_dispatch();
    }
    irq_leave_protection(state);
    return 0;
}

void task_dispatch(void)
{
    irq_state_t state = irq_enter_protection();
    task_t *to = task_next_run();
    if (to != task_manager.curr_task)
    {
        task_t *from = task_current();

        task_manager.curr_task = to;
        to->state = TASK_RUNNING;
        task_switch_from_to(from, to);
    }
    irq_leave_protection(state);
}

void task_time_tick(void)
{
    task_t *curr_task = task_current();
    if (--curr_task->slice_ticks == 0)
    {
        curr_task->slice_ticks = curr_task->time_ticks;
        task_set_block(curr_task);
        task_set_ready(curr_task);
        task_dispatch();
    }
    list_node_t *curr = list_first(&task_manager.sleep_list);
    while (curr)
    {
        list_node_t *next = list_node_next(curr);
        task_t *task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0)
        {
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }
    task_dispatch();
}
void sys_sleep(uint32_t ms)
{
    irq_state_t state = irq_enter_protection();

    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + OS_TICKS_MS - 1) / OS_TICKS_MS);
    task_dispatch();

    irq_leave_protection(state);
}
void task_set_sleep(task_t *task, uint32_t ticks)
{
    if (ticks == 0)
    {
        return;
    }
    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}
void task_set_wakeup(task_t *task)
{
    list_remove(&task_manager.sleep_list, &task->run_node);
}