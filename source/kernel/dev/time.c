#include "dev/time.h"
#include "cpu/irq.h"
#include "comm/cpu_int.h"
#include "os_conf.h"
#include "core/task.h"

static uint32_t sys_tick; // 系统启动后的tick数量

// 获取系统启动后到现在的时间节拍计数
uint32_t sys_get_ticks(void)
{
    return sys_tick;
}

// 定时器中断处理函数

void do_handler_time(exception_frame_t *frame)
{
    sys_tick++;
    pic_send_eoi(IRQ0_TIMER); // 需要先运行才能继续响应中断，不然执行后续代码可能会切换进程就不会继续相应中断了
    task_time_tick();
}

// 初始化硬件定时器
static void init_pit(void)
{
    uint32_t reload_count = PIT_OSC_FREQ / (1000.0 / OS_TICKS_MS);

    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE0);
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF);        // 加载低8位
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF); // 再加载高8位

    irq_install(IRQ0_TIMER, (irq_handler_t)exception_handler_time);
    irq_enable(IRQ0_TIMER);
}

// 定时器初始化

void time_init(void)
{
    sys_tick = 0;

    init_pit();
}
