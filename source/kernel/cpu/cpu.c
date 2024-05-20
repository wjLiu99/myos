#include "cpu/cpu.h"
#include "os_conf.h"
#include"comm/cpu_int.h"

static seg_desc_t gdt[GDT_SIZE];

// 设置段描述符
void seg_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr)
{
    seg_desc_t *desc = gdt + (selector >> 3);
    	if (limit > 0xfffff) {
		attr |= 0x8000;
		limit /= 0x1000;
	}
    desc->limit15_0 = limit & 0xffff;
    desc->addr15_0 = base & 0xffff;
    desc->addr16_23 = (base >> 16) & 0xff;
    desc->attr = attr | (((limit >> 16) & 0xf) << 8);
    desc->addr31_24 = (base >> 24) & 0xff;
}

// 设置中断门描述符
void gate_desc_set(gate_desc_t *desc, uint16_t selector, uint32_t offset, uint16_t attr)
{
    desc->offset15_0 = offset & 0xffff;
    desc->seg_selector = selector;
    desc->attr = attr;
    desc->offset31_16 = (offset >> 16) & 0xffff;
}
void init_gdt(void)
{
    // 清空段描述符表
    for (int i = 0; i < GDT_SIZE; i++)
    {
        seg_desc_set(i << 3, 0, 0, 0);
    }
    // 代码段
    seg_desc_set(CS_SELECTOR, 0x00000000, 0xffffffff,
                 SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D | SEG_G);
    // 数据段
    seg_desc_set(DS_SELECTOR, 0x00000000, 0xffffffff,
                 SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D | SEG_G);

    lgdt((uint32_t)gdt, sizeof(gdt));
}

void cpu_init()
{
    init_gdt();
    
}