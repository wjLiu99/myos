#include "core/memory.h"
#include "tools/log.h"
#include "tools/kernellib.h"
#include "cpu/mmu.h"

static addr_alloc_t paddr_alloc;
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE)));
// 初始化内存分配
static void addr_alloc_init(addr_alloc_t *m_alloc, uint8_t *bits, uint32_t start, uint32_t size, uint32_t page_size)
{
    mutex_init(&m_alloc->mutex);
    m_alloc->start = start;
    m_alloc->size = size;
    m_alloc->page_size = page_size;
    bitmap_init(&m_alloc->bitmap, bits, m_alloc->size / page_size, 0); // 0表示未分配
}

// 分配count页内存
static uint32_t addr_alloc_page(addr_alloc_t *alloc, int page_count)
{
    uint32_t addr = 0;
    mutex_lock(&alloc->mutex);
    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if (page_index >= 0)
    {
        addr = alloc->start + page_index * alloc->page_size;
    }
    mutex_unlock(&alloc->mutex);
    return addr;
}
// 释放内存
static void addr_free_page(addr_alloc_t *alloc, uint32_t addr, int page_count)
{
    mutex_lock(&alloc->mutex);
    int page_index = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, page_index, page_count, 0);

    mutex_unlock(&alloc->mutex);
}

// 打印内存信息
void show_mem_info(boot_info_t *boot_info)
{
    log_printf("mem region: ");
    for (int i = 0; i < boot_info->ram_region_count; i++)
    {
        log_printf("[%d]: start:0x%x size: 0x%x", i,
                   boot_info->ram_region_cfg[i].start,
                   boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

// 计算内存总容量
static uint32_t total_mem_size(boot_info_t *boot_info)
{
    uint32_t mem_size = 0;
    for (int i = 0; i < boot_info->ram_region_count; i++)
    {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}
pte_t *find_pte(pde_t *page_dir, uint32_t vaddr, int alloc)
{
    pte_t *page_table;
    pde_t *pde = page_dir + pde_index(vaddr);
    if (pde->present)
    {
        page_table = (pte_t *)pde_paddr(pde);
    }
    else
    {
        if (alloc == 0)
        {
            return (pte_t *)0;
        }

        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);
        if (pg_paddr == 0)
        {
            return (pte_t *)0;
        }
        pde->v = pg_paddr | PTE_P | PTE_W | PDE_U;
        page_table = (pte_t *)pg_paddr;
        kernel_memset(page_table, 0, MEM_PAGE_SIZE);
    }
    return page_table + pte_index(vaddr);
}

// 建立虚拟内存到物理内存的映射关系
int memory_create_map(pde_t *page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm)
{
    for (int i = 0; i < count; i++)
    {
        // log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);
        pte_t *pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0)
        {
            // log_printf("create pte failed. pte == 0");
            return -1;
        }
        // log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);
        pte->v = paddr | perm | PTE_P;
        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }
}

// 建立内核页表
void create_kernel_table(void)
{
    extern uint8_t s_text[], e_text[], s_data[];
    static memory_map_t kernel_map[] = {

        {0, s_text, 0, PTE_W},
        {s_text, e_text, s_text, 0},
        {s_data, (void *)MEM_EBDA_START, s_data, PTE_W},
        {(void *)MEM_EXT_START, (void *)MEM_EXT_END, (void *)MEM_EXT_START, PTE_W}};

    for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++)
    {
        memory_map_t *map = kernel_map + i;
        uint32_t vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        uint32_t vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        // 物理地址也要对其页边界
        uint32_t paddr = down2((uint32_t)map->pstart, MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, (uint32_t)map->pstart, page_count, map->perm);
    }
}

void memory_init(boot_info_t *boot_info)
{

    extern uint8_t *mem_free_start;
    uint8_t *mem_free = (uint8_t *)&mem_free_start;
    log_printf("mem init");
    show_mem_info(boot_info);

    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);

    log_printf("free memory : start 0x%x ,size :0x%x", MEM_EXT_START, mem_up1MB_free);

    // 地址分配结构管理1m以上空间
    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);

    mem_free += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    create_kernel_table();

    mmu_set_page_dir((uint32_t)kernel_page_dir);
}

// 建立用户空间虚拟内存到物理地址空间映射，写进页表中,完成内核空间映射
uint32_t memory_create_uvm(void)
{
    pde_t *page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0)
    {
        return 0;
    }

    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    // 用户空间起始地址对应的页目录表项
    uint32_t user_pde_start = pde_index(MEM_TASK_BASE);
    for (int i = 0; i < user_pde_start; i++)
    {
        // 内核空间直接使用内核空间的页表，节省空间
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}

// 给特定页表分配一页空间
int memory_alloc_for_page_dir(uint32_t page_dir, uint32_t vaddr, uint32_t size, int perm)
{
    uint32_t curr_vaddr = vaddr;
    int page_count = up2(size, MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    for (int i = 0; i < page_count; i++)

    {
        uint32_t paddr = addr_alloc_page(&paddr_alloc, 1);
        vaddr = down2(vaddr, MEM_PAGE_SIZE);
        if (paddr == 0)
        {
            log_printf("mem alloc failed.no memory");
            return 0;
        }

        int err = memory_create_map((pde_t *)page_dir, curr_vaddr, paddr, 1, perm);
        if (err < 0)
        {
            log_printf("create memory faild.");
            // 已经建立的映射关系未处理
            addr_free_page(&paddr_alloc, vaddr, i + 1);
            return 0;
        }

        curr_vaddr += MEM_PAGE_SIZE;
    }
    return 0;
}

// 给用户空间分配内存,使用tss段里保存的进程自己的页目录表，给指定的虚拟地址分配一页物理内存
// 主要是用户空间8开头以上的地址,为当前运行的任务
int memory_alloc_page_for(uint32_t addr, uint32_t size, int perm)
{
    return memory_alloc_for_page_dir(task_current()->tss.cr3, addr, size, perm);
}

// 8开头以下的地址
uint32_t memory_alloc_page(void)
{
    // 内核空间虚拟地址与物理地址相同,addr_alloc_page返回的是物理地址
    return addr_alloc_page(&paddr_alloc, 1);
}

static pde_t *curr_page_dir(void)
{
    return (pde_t *)(task_current()->tss.cr3);
}
void memory_free_page(uint32_t addr)
{
    // memory_alloc_page分配的直接释放，memory_alloc_page_for分配已经建立了页表映射
    if (addr < MEM_TASK_BASE)
    {
        addr_free_page(&paddr_alloc, addr, 1);
    }
    else
    {
        pte_t *pte = find_pte(curr_page_dir(), addr, 0);
        // ASSERT((pte == (pte_t *)0) && pte->present);
        addr_free_page(&paddr_alloc, pte_addr(pte), 1);
        pte->v = 0;
    }
}

void memory_destory_uvm(uint32_t page_dir)
{
    // 内核空间映射所有进程都是一样的所以不能释放物理空间
    uint32_t user_pde_start = pde_index(MEM_TASK_BASE);
    pde_t *pde = (pde_t *)page_dir + user_pde_start;
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++)
    {
        if (!pde->present)
        {
            continue;
        }
        pte_t *pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++)
        {
            if (!pte->present)
            {
                continue;
            }
            addr_free_page(&paddr_alloc, pte_addr(pte), 1);
        }
        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);
    }
    addr_free_page(&paddr_alloc, page_dir, 1);
}
uint32_t memory_copy_uvm(uint32_t page_dir)
{
    uint32_t to_page_dir = memory_create_uvm();
    if (to_page_dir == 0)
    {
        goto copy_uvm_failed;
    }

    uint32_t user_pde_start = pde_index(MEM_TASK_BASE);
    pde_t *pde = (pde_t *)page_dir + user_pde_start;

    for (int i = user_pde_start; i < PDE_CNT; i++, pde++)
    {
        if (!pde->present)
        {
            continue;
        }

        pte_t *pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++)
        {
            if (!pte->present)
            {
                continue;
            }
            uint32_t page = addr_alloc_page(&paddr_alloc, 1);
            if (page == 0)
            {
                goto copy_uvm_failed;
            }
            uint32_t vaddr = (i << 22) | (j << 12);
            int err = memory_create_map((pde_t *)to_page_dir, vaddr, page, 1, get_pte_perm(pte));
            if (err < 0)
            {
                goto copy_uvm_failed;
            }

            kernel_memcpy((void *)page, (void *)vaddr, MEM_PAGE_SIZE);
        }
    }
    return to_page_dir;

copy_uvm_failed:
    if (to_page_dir)
    {
        memory_destory_uvm(to_page_dir);
    }
    return -1;
}

uint32_t memory_get_paddr(uint32_t page_dir, uint32_t vaddr)
{
    pte_t *pte = find_pte((pde_t *)page_dir, vaddr, 0);
    if (!pte)
    {
        return 0;
    }
    return pte_addr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}

// from是在当前页表中，所以进程看到的是连续空间，to是未启用的页表的地址，所以要找到他在页表中的物理地址，再根据在当前页表中虚拟地址和物理地址相同拷贝
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size)
{
    while (size > 0)
    {
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if (to_paddr == 0)
        {
            return -1;
        }
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE - 1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        // 剩余要拷贝的大小要小于当前页剩余的空间，调整拷贝大小
        if (curr_size > size)
        {
            curr_size = size;
        }

        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);
        size -= curr_size;
        to += curr_size;
        from += curr_size;
    }
}