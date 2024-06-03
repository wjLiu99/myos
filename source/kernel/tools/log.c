#include "tools/log.h"
#include "comm/cpu_int.h"
#include "stdarg.h"
#include "tools/kernellib.h"
#include "cpu/irq.h"
#include "ipc/mutex.h"
#include "dev/console.h"
#include "dev/dev.h"
// 是否利用qemu提供的串行接口打印日志信息
#define LOG_USE_COM 0
#define COM1_PORT 0x3F8 // RS232端口0初始化

static mutex_t mutex;
static int log_dev_id;
void log_init(void)
{
    // 串行接口初始化
    mutex_init(&mutex);
    log_dev_id = dev_open(DEV_TTY, 0, (void *)0);
#if LOG_USE_COM
    outb(COM1_PORT + 1, 0x00); // 关闭串行接口内部的中断
    outb(COM1_PORT + 3, 0x80); // 发送速度配置
    outb(COM1_PORT + 0, 0x03); //
    outb(COM1_PORT + 1, 0x00); //
    outb(COM1_PORT + 3, 0x03); //
    outb(COM1_PORT + 2, 0xC7); //
#endif
}
void log_printf(const char *fmt, ...)
{
    char str_buf[128];
    va_list args;
    kernel_memset(str_buf, '\0', sizeof(str_buf));
    va_start(args, fmt);
    kernel_vsprintf(str_buf, fmt, args);
    va_end(args);
    mutex_lock(&mutex);
#if LOG_USE_COM
    const char *p = str_buf;
    while (*p != '\0')
    {
        while ((inb(COM1_PORT + 5) & (1 << 6)) == 0)
            ; // 串行接口忙
        outb(COM1_PORT, *p++);
    }
    // 不同系统终端处理不同
    outb(COM1_PORT, '\n'); // 改变列号
    outb(COM1_PORT, '\r'); // 改变行号
#else
    // console_write(0, str_buf, kernel_strlen(str_buf));
    dev_write(log_dev_id, 0, str_buf, kernel_strlen(str_buf));
    char c = '\n';
    // console_write(0, &c, 1);
    dev_write(log_dev_id, 0, &c, 1);
#endif
    mutex_unlock(&mutex);
}