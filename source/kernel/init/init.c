#include "init.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "comm/boot_info.h"
#include "os_conf.h"
#include "tools/kernellib.h"
#include "core/task.h"
#include "comm/cpu_int.h"
#include "tools/list.h"

void kernel_init(boot_info_t *boot_info)
{

    cpu_init();
    log_init();
    irq_init();
    time_init();
}

static task_t init_task;
void init_task_entry(void)
{
    int cnt = 0;
    while (1)
    {
        log_printf("init task : %d", cnt++);
        sys_sched_yield();
    }
}
static uint32_t init_task_stack[1024];

void list_test()
{
    list_t list;
    list_init(&list);
    log_printf("list:first:=0x%x , last = 0x%x, count =%d",list_first(&list),list_last(&list),list_count(&list));
}
void init_main(void)
{
    list_test();

    log_printf("kernel is running...");
    log_printf("Version: %s", OS_VERSION);
    log_printf("%d %d %x %c", 123, -123, 100, 'a');
    int cnt = 0;
    // irq_enable_global();
    // int a = 3/0;
    task_init(&init_task, "init task",(uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    
    task_first_init();
    while (1)
    {
        log_printf("init main : %d", cnt++);
        sys_sched_yield();
    }
}

// 8259 8253