#ifndef CPU_H
#define CPU_H
#include"comm/types.h"

//gdt表标志位
#define SEG_G				(1 << 15)		// 设置段界限的单位，1-4KB，0-字节
#define SEG_D				(1 << 14)		// 控制是否是32位、16位的代码或数据段
#define SEG_P_PRESENT	    (1 << 7)		// 段是否存在

#define SEG_DPL0			(0 << 5)		// 特权级0，最高特权级
#define SEG_DPL3			(3 << 5)		// 特权级3，最低权限

#define SEG_S_SYSTEM		(0 << 4)		// 是否是系统段，如调用门或者中断
#define SEG_S_NORMAL		(1 << 4)		// 普通的代码段或数据段

#define SEG_TYPE_CODE		(1 << 3)		// 指定其为代码段
#define SEG_TYPE_DATA		(0 << 3)		// 数据段

#define SEG_TYPE_RW			(1 << 1)		// 是否可写可读，不设置为只读

#define SEG_TYPE_TSS      	(9 << 0)		// 32位TSS

#define GATE_TYPE_IDT		(0xE << 8)		// 中断32位门描述符
#define GATE_TYPE_SYSCALL	(0xC << 8)		// 调用门
#define GATE_P_PRESENT		(1 << 15)		// 是否存在
#define GATE_DPL0			(0 << 13)		// 特权级0，最高特权级
#define GATE_DPL3			(3 << 13)		// 特权级3，最低权限

#define SEG_RPL0                (0 << 0)
#define SEG_RPL3                (3 << 0)

#define EFLAGS_IF           (1 << 9)
#define EFLAGS_DEFAULT      (1 << 1)

#pragma pack(1)
//段描述符结构体
typedef struct seg_desc_t{
    uint16_t limit15_0;
    uint16_t addr15_0;
    uint8_t addr16_23;
    uint16_t attr;//段属性
    uint8_t addr31_24;
}seg_desc_t;
void cpu_init (void);
void seg_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr);
#pragma pack()
#endif