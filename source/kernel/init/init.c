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
#include "ipc/sem.h"

void kernel_init(boot_info_t *boot_info)
{

    cpu_init();
    log_init();
    irq_init();
    time_init();
    task_manager_init();
}

static task_t init_task;
static sem_t sem;
void init_task_entry(void)
{
    int cnt = 0;
    while (1)
    {
        sem_wait(&sem);
        log_printf("init task : %d", cnt++);
    }
}
static uint32_t init_task_stack[1024];

void init_main(void)
{

    log_printf("kernel is running...");
    log_printf("Version: %s", OS_VERSION);
    log_printf("%d %d %x %c", 123, -123, 100, 'a');
    int cnt = 0;

    // int a = 3/0;
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1023]);
    // task_set_ready(&init_task);
    task_first_init();
    sem_init(&sem, 0);
    irq_enable_global();

    while (1)
    {
        log_printf("init main : %d", cnt++);
        sem_wakeup(&sem);
        sys_sleep(1000);
    }
}

// 8259 8253