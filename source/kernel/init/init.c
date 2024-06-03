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
#include "core/memory.h"
#include "dev/console.h"
#include "dev/kbd.h"
#include "fs/fs.h"

void kernel_init(boot_info_t *boot_info)
{

    cpu_init();
    irq_init();
    log_init();
    // console_init();
    memory_init(boot_info);
    fs_init();

    time_init();
    task_manager_init();
    // kbd_init();
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

void move_to_first_task(void)
{
    task_t *curr = task_current();
    tss_t *tss = &(curr->tss);
    __asm__ __volatile__(
        // 模拟中断返回，切换入第1个可运行应用进程
        // 不过这里并不直接进入到进程的入口，而是先设置好段寄存器，再跳过去
        "push %[ss]\n\t"     // SS
        "push %[esp]\n\t"    // ESP
        "push %[eflags]\n\t" // EFLAGS
        "push %[cs]\n\t"     // CS
        "push %[eip]\n\t"    // ip
        "iret\n\t" ::[ss] "r"(tss->ss),
        [esp] "r"(tss->esp), [eflags] "r"(tss->eflags),
        [cs] "r"(tss->cs), [eip] "r"(tss->eip));
}

void init_main(void)
{

    log_printf("kernel is running...");
    log_printf("Version: %s", OS_VERSION);
    log_printf("%d %d %x %c", 123, -123, 100, 'a');
    int cnt = 0;

    // task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1023]);

    task_first_init();
    sem_init(&sem, 0);

    move_to_first_task();
    // irq_enable_global();

    // while (1)
    // {
    //     log_printf("init main : %d", cnt++);
    //     sem_wakeup(&sem);
    //     sys_sleep(1000);
    // }
}

// 8259 8253