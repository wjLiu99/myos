#include "cpu/cpu.h"
#include "os_conf.h"
static seg_desc_t gdt[GDT_SIZE];

//设置段描述符
void seg_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr)
{
    seg_desc_t *desc = gdt + (selector >> 3);
    desc->limit15_0 = limit & 0xffff;
    desc->addr15_0 = base & 0xffff;
    desc->addr16_23 = (base >> 16) & 0xff;
    desc->attr = attr | (((limit >> 16) & 0xf) << 8);
    desc->addr31_24 = (base >> 24) & 0xff;
}

void init_gdt(void){
    for (int i = 0; i < GDT_SIZE; i++) {
        seg_desc_set(i << 3, 0, 0, 0);
    }

}

void cpu_init(){

}