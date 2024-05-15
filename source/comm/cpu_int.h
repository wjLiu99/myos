#ifndef CPU_INT_H
#define CPU_INT_H
#include"types.h"
//关中断
static inline void cli(void){
    __asm__ __volatile__("cli");
}
//开中断
static inline void sti(void){
    __asm__ __volatile__("sti");
}
//读端口
static inline uint8_t inb(uint16_t port){
    uint8_t ret;
    __asm__ __volatile__("inb %[p],%[r]":[r]"=a"(ret):[p]"d"(port));
    return ret;

}

//写端口
static inline void outb(uint16_t port,uint8_t data){

    __asm__ __volatile__("outb %[d],%[p]"::[d]"a"(data),[p]"d"(port));
}

//加载gdt表
static inline void lgdt(uint32_t start ,uint32_t size){
    struct{
        uint16_t limit;
        uint32_t start0_15;
        uint32_t start16_31;
    }gdt;
    gdt.limit = size -1;
    gdt.start0_15 = start >>16;
    gdt.start16_31 = start & 0xFFFF;

    __asm__ __volatile__("lgdt %[g]"::[g]"m"(gdt));
}

#endif