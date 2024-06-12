// 操作系统配置
#ifndef OS_CONF_H
#define OS_CONF_H

#define GDT_SIZE 256             // 段描述符表项数量
#define CS_SELECTOR (1 * 8)      // 代码段描述符
#define DS_SELECTOR (2 * 8)      // 数据段描述符
#define SYSCALL_SELECTOR (3 * 8) // 调用门描述符

#define KERNEL_STACK_SIZE (8 * 1024)

#define TASK_NR 128

#define OS_TICKS_MS 10 // 定时器每隔10ms产生一次中断

#define OS_VERSION "1.0.0"

#define ROOT_DEV DEV_DISK, 0xb1

#endif