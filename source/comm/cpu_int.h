#ifndef CPU_INT_H
#define CPU_INT_H
#include "types.h"
// 关中断
static inline void cli(void)
{
    __asm__ __volatile__("cli");
}
// 开中断
static inline void sti(void)
{
    __asm__ __volatile__("sti");
}
// 读端口
// static inline uint8_t inb(uint16_t port)
// {
//     uint8_t ret;
//     __asm__ __volatile__("inb %[p],%[r]" : [r] "=a"(ret) : [p] "d"(port));
//     return ret;
// }

// static inline uint16_t inw(uint16_t port)
// {
//     uint16_t rv;
//     __asm__ __volatile__("in %1, %0" : "=a"(rv) : "dN"(port));
//     return rv;
// }
// 写端口
// static inline void outb(uint16_t port, uint8_t data)
// {

//     __asm__ __volatile__("outb %[d],%[p]" ::[d] "a"(data), [p] "d"(port));
// }
// static inline void outw(uint16_t port, uint16_t data)
// {

//     __asm__ __volatile__("out %[d],%[p]" ::[d] "a"(data), [p] "d"(port));
// }
static inline uint8_t inb(uint16_t port)
{
    uint8_t rv;
    __asm__ __volatile__("inb %[p], %[v]" : [v] "=a"(rv) : [p] "d"(port));
    return rv;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t rv;
    __asm__ __volatile__("in %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t rv;
    __asm__ __volatile__("inl %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %[v], %[p]" : : [p] "d"(port), [v] "a"(data));
}

static inline void outw(uint16_t port, uint16_t data)
{
    __asm__ __volatile__("out %[v], %[p]" : : [p] "d"(port), [v] "a"(data));
}

static inline void outl(uint16_t port, uint32_t data)
{
    __asm__ __volatile__("outl %[v], %[p]" : : [p] "d"(port), [v] "a"(data));
}
// 加载全局描述符表GDT
static inline void lgdt(uint32_t start, uint32_t size)
{
    struct
    {
        uint16_t limit;
        uint16_t start0_15;
        uint16_t start16_31;
    } gdt;
    gdt.limit = size - 1;
    gdt.start0_15 = start & 0xFFFF;
    gdt.start16_31 = start >> 16;

    __asm__ __volatile__("lgdt %[g]" ::[g] "m"(gdt));
}

static inline void lidt(uint32_t start, uint32_t size)
{
    struct
    {
        uint16_t limit;
        uint16_t start0_15;
        uint16_t start16_31;
    } idt;
    idt.limit = size - 1;
    idt.start0_15 = start & 0xFFFF;
    idt.start16_31 = start >> 16;

    __asm__ __volatile__("lidt %[g]" ::[g] "m"(idt));
}
// 暂停
static inline void hlt(void)
{
    __asm__ __volatile__("hlt");
}
// 读取cr0寄存器
static inline uint32_t r_cr0(void)
{
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0 , %[v]" : [v] "=r"(cr0));
    return cr0;
}
// 写入cr0寄存器
static inline void w_cr0(uint32_t data)
{

    __asm__ __volatile__("mov %[v] , %%cr0" ::[v] "r"(data));
}
// 读取cr2寄存器
static inline uint32_t r_cr2(void)
{
    uint32_t cr2;
    __asm__ __volatile__("mov %%cr2 , %[v]" : [v] "=r"(cr2));
    return cr2;
}
// 写入cr2寄存器
static inline void w_cr2(uint32_t data)
{

    __asm__ __volatile__("mov %[v] , %%cr2" ::[v] "r"(data));
}
// 读取cr3寄存器
static inline uint32_t r_cr3(void)
{
    uint32_t cr3;
    __asm__ __volatile__("mov %%cr3 , %[v]" : [v] "=r"(cr3));
    return cr3;
}
// 写入cr3寄存器
static inline void w_cr3(uint32_t data)
{

    __asm__ __volatile__("mov %[v] , %%cr3" ::[v] "r"(data));
}

// 读取cr4寄存器
static inline uint32_t r_cr4(void)
{
    uint32_t cr4;
    __asm__ __volatile__("mov %%cr3 , %[v]" : [v] "=r"(cr4));
    return cr4;
}
// 写入cr4寄存器
static inline void w_cr4(uint32_t data)
{

    __asm__ __volatile__("mov %[v] , %%cr4" ::[v] "r"(data));
}
// 远跳转
static inline void jmp_far_ptr(uint32_t selector, uint32_t offset)
{
    uint32_t addr[] = {offset, selector};
    __asm__ __volatile__("ljmpl *(%[a]) " ::[a] "r"(addr));
}

static inline void write_tr(uint16_t tss_sel)
{
    __asm__ __volatile__("ltr %%ax" ::"a"(tss_sel));
}

static inline uint32_t read_eflags(void)
{
    uint32_t eflags;
    __asm__ __volatile__("pushf\n\tpop %%eax" : "=a"(eflags));
    return eflags;
}
static inline void write_eflags(uint32_t eflags)
{

    __asm__ __volatile__("push %%eax\n\tpopf" ::"a"(eflags));
}
#endif