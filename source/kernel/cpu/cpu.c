#include "cpu/cpu.h"
#include "os_conf.h"
#include "comm/cpu_int.h"
#include "cpu/irq.h"
#include "ipc/mutex.h"
#include "core/syscall.h"
static seg_desc_t gdt[GDT_SIZE];
static mutex_t mutex;

// 设置段描述符
void seg_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr)
{
    seg_desc_t *desc = gdt + (selector >> 3);
    if (limit > 0xfffff)
    {
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

    gate_desc_set((gate_desc_t *)(gdt + (SYSCALL_SELECTOR >> 3)), CS_SELECTOR, (uint32_t)exception_handler_syscall,
                  GATE_P_PRESENT | GATE_DPL3 | GATE_TYPE_SYSCALL | SYSCALL_PARAM_COUNT);

    lgdt((uint32_t)gdt, sizeof(gdt));
}

void cpu_init()
{
    mutex_init(&mutex);
    init_gdt();
}

int gdt_alloc_desc(void)
{
    mutex_lock(&mutex);
    for (int i = 1; i < GDT_SIZE; i++)
    {
        seg_desc_t *desc = gdt + i;
        if (desc->attr == 0)
        {
            mutex_unlock(&mutex);
            return i * sizeof(seg_desc_t);
        }
    }
    mutex_unlock(&mutex);
    return -1;
}
void gdt_free_sel(int sel)
{
    mutex_lock(&mutex);
    gdt[sel / sizeof(seg_desc_t)].attr = 0;
    mutex_unlock(&mutex);
}

void switch_to_tss(uint32_t tss_selector)
{
    jmp_far_ptr(tss_selector, 0);
}