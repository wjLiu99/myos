#include "tools/log.h"
#include "comm/cpu_int.h"
#include"stdarg.h"
#include"tools/kernellib.h"
#define COM1_PORT 0x3F8 // RS232端口0初始化
//利用qemu提供的串行接口打印日志信息
void log_init(void)
{
    // 串行接口初始化
    outb(COM1_PORT + 1, 0x00); // 关闭串行接口内部的中断
    outb(COM1_PORT + 3, 0x80); // 发送速度配置
    outb(COM1_PORT + 0, 0x03); //
    outb(COM1_PORT + 1, 0x00); //
    outb(COM1_PORT + 3, 0x03); //
    outb(COM1_PORT + 2, 0xC7); //
}
void log_printf(const char *fmt, ...)
{   
    char str_buf[128];
    va_list args;
    kernel_memset(str_buf,'\0',sizeof(str_buf));
    va_start(args,fmt);
    kernel_vsprintf(str_buf,fmt,args);
    va_end(args);

    const char *p = str_buf;
    while (*p != '\0')
    {
        while ((inb(COM1_PORT + 5) & (1 << 6)) == 0)
            ; // 串行接口忙
        outb(COM1_PORT, *p++);
    }
    //不同系统终端处理不同
    outb(COM1_PORT,'\n'); //改变列号
    outb(COM1_PORT,'\r'); //改变行号
}