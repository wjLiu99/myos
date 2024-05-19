//操作系统配置
#ifndef OS_CONF_H
#define OS_CONF_H

#define GDT_SIZE 256 //段描述符表项数量
#define CS_SELECTOR	    (1 * 8)		// 代码段描述符
#define DS_SELECTOR		(2 * 8)		// 数据段描述符

#define KERNEL_STACK_SIZE (8*1024)
#endif