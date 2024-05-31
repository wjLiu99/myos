#include "core/task.h"
#include "tools/kernellib.h"
#include "os_conf.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_int.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/mmu.h"
#include "core/syscall.h"
#include "comm/elf.h"
#include "fs/fs.h"

static task_manager_t task_manager;
static task_t task_table[TASK_NR];
static mutex_t task_table_mutex;
static int tss_init(task_t *task, int flag, uint32_t entry, uint32_t esp)
{
    // 为TSS分配GDT
    int tss_sel = gdt_alloc_desc();
    if (tss_sel < 0)
    {
        log_printf("alloc tss failed.\n");
        return -1;
    }
    seg_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t),
                 SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    task->tss_sel = tss_sel;
    kernel_memset(&task->tss, 0, sizeof(tss_t));

    int code_sel, data_sel;
    if (flag & TASK_FLAGS_SYSTEM)
    {
        code_sel = CS_SELECTOR;
        data_sel = DS_SELECTOR;
    }
    else
    {
        code_sel = task_manager.app_code_sel | SEG_RPL3;
        data_sel = task_manager.app_data_sel | SEG_RPL3;
    }
    // 用于异常处理的内核栈,每个任务有一个自己的内核栈空间
    uint32_t kernel_stack = memory_alloc_page();
    if (kernel_stack == 0)
    {
        goto tss_init_failed;
    }

    task->tss.eip = entry;
    task->tss.esp = esp; // 未指定栈则用内核栈，即运行在特权级0的进程
    task->tss.esp0 = kernel_stack + MEM_PAGE_SIZE;
    task->tss.ss0 = DS_SELECTOR; // 特权0使用的栈
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds = task->tss.fs = task->tss.gs = data_sel; // 全部采用同一数据段
    task->tss.cs = code_sel;
    task->tss.iomap = 0;
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0)
    {
        goto tss_init_failed;
    }
    task->tss.cr3 = page_dir;
    return 0;
tss_init_failed:
    gdt_free_sel(tss_sel);
    if (kernel_stack)
    {
        memory_free_page(kernel_stack);
    }
    return -1;
}

int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp)
{

    task->pid = (uint32_t)task;
    task->parent = (task_t *)0;
    ASSERT(task != (task_t *)0);
    task->heap_start = 0;
    task->heap_end = 0;
    tss_init(task, flag, entry, esp);
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->time_ticks = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_ticks;
    task->sleep_ticks = 0;
    // 栈是先减再压数据
    //  uint32_t *pesp =  (uint32_t *)esp;
    //  if(pesp){
    //      *(--pesp) = entry;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      *(--pesp)=0;
    //      task->stack = pesp;
    //  }
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    irq_state_t state = irq_enter_protection();

    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}

void task_uninit(task_t *task)
{
    if (task->tss_sel)
    {
        gdt_free_sel(task->tss_sel);
    }
    if (task->tss.esp0)
    {
        memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
    }
    if (task->tss.cr3)
    {
        memory_destory_uvm(task->tss.cr3);
    }

    kernel_memset(task, 0, sizeof(task_t));
}

void task_start(task_t *task)
{
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    irq_leave_protection(state);
}

void simple_switch(uint32_t **from, uint32_t *to);

void task_switch_from_to(task_t *from, task_t *to)
{
    switch_to_tss(to->tss_sel);
    // simple_switch(&from->stack, to->stack);
}

static void idle_task_entry(void)
{
    while (1)
    {
        hlt();
    }
}
static uint32_t idle_task_stack[1024];
void task_manager_init(void)
{
    int sel = gdt_alloc_desc();
    seg_desc_set(sel, 0x00000000, 0xffffffff, SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_data_sel = sel;
    sel = gdt_alloc_desc();
    seg_desc_set(sel, 0x00000000, 0xffffffff, SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = sel;
    list_init(&task_manager.task_list);
    list_init(&task_manager.ready_list);
    list_init(&task_manager.sleep_list);
    task_manager.curr_task = (task_t *)0;
    task_init(&task_manager.idle_task,
              "idle task",
              TASK_FLAGS_SYSTEM,
              (uint32_t)idle_task_entry,
              (uint32_t)(&idle_task_stack[1024]));
    task_start(&task_manager.idle_task);
    kernel_memset(task_table, 0, sizeof(task_table));
    mutex_init(&task_table_mutex);
}

void task_first_init(void)
{
    void first_task_entry(void);
    extern uint8_t s_first_task[], e_first_task[];

    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
    ASSERT(copy_size <= alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry;
    task_init(&task_manager.first_task, "first task", 0, first_start, (uint32_t)first_start + alloc_size); // 正在运行的进程不用设置入口地址
    task_manager.first_task.heap_start = (uint32_t)e_first_task;
    task_manager.first_task.heap_end = (uint32_t)e_first_task;
    task_manager.curr_task = &task_manager.first_task;

    // 现在cr3已经是进程自己的页表了，tss_init的时候给进程分配了新的页表
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // s_first_task是物理地址，但是页表中mem_ext_end以下的虚拟地址和物理地址完成了一一映射
    // fisrt_start是虚拟地址，在分配完新的页表项后以及完成映射，可以直接写,将代码搬运到0x80000000处
    memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W | PTE_U);
    kernel_memcpy((void *)first_start, s_first_task, copy_size);
    // 加入就绪队列
    task_start(&task_manager.first_task);
    write_tr(task_manager.first_task.tss_sel);
}

task_t *task_first_task(void)
{
    return &task_manager.first_task;
}

// 将任务从就绪队列移除
void task_set_block(task_t *task)
{
    if (task == &task_manager.idle_task)
    {
        return;
    }
    list_remove(&task_manager.ready_list, &task->run_node);
}

task_t *task_current(void)
{
    return task_manager.curr_task;
}

static task_t *task_next_run(void)
{

    if (list_count(&task_manager.ready_list) == 0)
    {
        return &task_manager.idle_task;
    }
    // 普通任务
    list_node_t *task_node = list_first(&task_manager.ready_list);
    return (task_t *)list_node_parent(task_node, task_t, run_node);
}

void task_set_ready(task_t *task)
{
    if (task == &task_manager.idle_task)
    {
        return;
    }
    list_insert_last(&task_manager.ready_list, &task->run_node);
    task->state = TASK_READY;
}

int sys_yield(void)
{
    irq_state_t state = irq_enter_protection();
    if (list_count(&task_manager.ready_list) > 1)
    {
        task_t *curr_task = task_current();
        task_set_block(curr_task);
        task_set_ready(curr_task);
        task_dispatch();
    }
    irq_leave_protection(state);
    return 0;
}

void task_dispatch(void)
{
    irq_state_t state = irq_enter_protection();
    task_t *to = task_next_run();
    if (to != task_manager.curr_task)
    {
        task_t *from = task_current();

        task_manager.curr_task = to;
        to->state = TASK_RUNNING;
        task_switch_from_to(from, to);
    }
    irq_leave_protection(state);
}

void task_time_tick(void)
{
    task_t *curr_task = task_current();
    if (--curr_task->slice_ticks == 0)
    {
        curr_task->slice_ticks = curr_task->time_ticks;
        task_set_block(curr_task);
        task_set_ready(curr_task);
        task_dispatch();
    }
    list_node_t *curr = list_first(&task_manager.sleep_list);
    while (curr)
    {
        list_node_t *next = list_node_next(curr);
        task_t *task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0)
        {
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }
    task_dispatch();
}
void sys_sleep(uint32_t ms)
{
    irq_state_t state = irq_enter_protection();

    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + OS_TICKS_MS - 1) / OS_TICKS_MS);
    task_dispatch();

    irq_leave_protection(state);
}
void task_set_sleep(task_t *task, uint32_t ticks)
{
    if (ticks == 0)
    {
        return;
    }
    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}
void task_set_wakeup(task_t *task)
{
    list_remove(&task_manager.sleep_list, &task->run_node);
}

int sys_getpid(void)
{
    task_t *task = task_current();
    return task->pid;
}

int sys_fork(void)
{
    task_t *parent_task = task_current();
    task_t *child_task = alloc_task();
    if (child_task == (task_t *)0)
    {
        goto fork_failed;
    }
    syscall_frame_t *frame = (syscall_frame_t *)(parent_task->tss.esp0 - sizeof(syscall_frame_t));

    int err = task_init(child_task, "child", 0, frame->eip, frame->esp + sizeof(uint32_t) * SYSCALL_PARAM_COUNT);
    if (err < 0)
    {
        goto fork_failed;
    }
    tss_t *tss = &child_task->tss;

    // 设置子进程返回值为0
    tss->eax = 0;
    tss->ebx = frame->ebx;
    tss->ecx = frame->ecx;
    tss->edx = frame->edx;
    tss->esi = frame->esi;
    tss->edi = frame->edi;
    tss->ebp = frame->ebp;

    tss->cs = frame->cs;
    tss->ds = frame->ds;
    tss->es = frame->es;
    tss->fs = frame->fs;
    tss->gs = frame->gs;
    tss->eflags = frame->eflags;
    child_task->parent = parent_task;

    if ((child_task->tss.cr3 = memory_copy_uvm(parent_task->tss.cr3)) < 0)
    {
        goto fork_failed;
    }
    task_start(child_task);
    return child_task->pid;

fork_failed:
    if (child_task)
    {
        task_uninit(child_task);
        free_task(child_task);
    }
    return -1;
}

// 分配任务结构体
static task_t *alloc_task(void)
{

    task_t *task = (task_t *)0;
    mutex_lock(&task_table_mutex);
    for (int i = 0; i < TASK_NR; i++)
    {
        task_t *curr = task_table + i;
        if (curr->name[0] == '\0')
        {
            task = curr;
            break;
        }
    }
    mutex_unlock(&task_table_mutex);
    return task;
}

static void free_task(task_t *task)
{
    mutex_lock(&task_table_mutex);
    task->name[0] = '\0';
    mutex_unlock(&task_table_mutex);
}

static int load_phdr(int file, Elf32_Phdr *phdr, uint32_t page_dir)
{
    int err = memory_alloc_for_page_dir(page_dir, phdr->p_vaddr, phdr->p_memsz, PTE_P | PTE_U | PTE_W);
    if (err < 0)
    {
        log_printf("no mem");
        return -1;
    }
    if (sys_lseek(file, phdr->p_offset, 0) < 0)
    {
        log_printf("read file failed");
        return -1;
    }
    uint32_t vaddr = phdr->p_vaddr;
    uint32_t size = phdr->p_filesz;
    // 当前页表还是调用exec函数的进程页表，新页表还未启用,一页一页拷贝，因为在物理空间中并不连续存放
    while (size > 0)
    {
        int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;
        // 找到vaddr在pagedir中的物理地址,将文件读到物理地址中，物理地址和虚拟地址一一映射，但是进程空间虚拟地址可以和内核映射到相同的物理地址
        uint32_t paddr = memory_get_paddr(page_dir, vaddr);
        if (sys_read(file, (char *)paddr, curr_size) < curr_size)
        {
            log_printf("read file failed");
            return -1;
        }
        size -= curr_size;
        vaddr += curr_size;
    }
    return 0;
}
static uint32_t load_elf_file(task_t *task, const char *name, uint32_t page_dir)
{
    Elf32_Ehdr elf_hdr;
    Elf32_Phdr elf_phdr;
    int file = sys_open(name, 0);
    if (file < 0)
    {
        log_printf("open failed.%s", name);
        goto load_failed;
    }

    int cnt = sys_read(file, (char *)&elf_hdr, sizeof(elf_hdr));
    if (cnt < sizeof(Elf32_Ehdr))
    {
        log_printf("elf hdr too small");
        goto load_failed;
    }

    if ((elf_hdr.e_ident[0] != 0x7f) || (elf_hdr.e_ident[1] != 'E') ||
        (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F'))
    {
        log_printf("check elf failed");
        goto load_failed;
    }

    uint32_t e_phoff = elf_hdr.e_phoff;
    for (int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize)
    {
        if (sys_lseek(file, e_phoff, 0) < 0)
        {
            log_printf("read file failed");
            goto load_failed;
        }
        cnt = sys_read(file, (char *)&elf_phdr, sizeof(elf_phdr));
        if (cnt < sizeof(elf_phdr))
        {
            log_printf("elf_phdr small");
            goto load_failed;
        }
        if ((elf_phdr.p_type != 1) || (elf_phdr.p_vaddr < MEM_TASK_BASE))
        {
            continue;
        }
        // 从文件中加载到内存中
        int err = load_phdr(file, &elf_phdr, page_dir);
        if (err < 0)
        {
            log_printf("load program failed");
            goto load_failed;
        }
        task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
        task->heap_end = task->heap_start;
    }
    sys_close(file);
    return elf_hdr.e_entry;

load_failed:
    if (file)
    {
        sys_close(file);
    }
    return 0;
}

static int copy_args(char *to, uint32_t page_dir, int argc, char **argv)
{
    task_args_t task_args;
    task_args.argc = argc;
    task_args.argv = (char **)(to + sizeof(task_args_t));
    // 拷贝参数的起始虚拟地址
    char *dest_arg = to + sizeof(task_args_t) + sizeof(char *) * argc;
    // 找到新进程argv的物理地址
    char **dest_arg_tb = (char **)memory_get_paddr(page_dir, (uint32_t)(to + sizeof(task_args_t)));
    for (int i = 0; i < argc; i++)
    {
        char *from = argv[i];
        int len = kernel_strlen(from) + 1;
        // 不同页表之间数据拷贝
        int err = memory_copy_uvm_data((uint32_t)dest_arg, page_dir, (uint32_t)from, len);
        ASSERT(err >= 0);
        dest_arg_tb[i] = dest_arg;
        dest_arg += len;
    }
    memory_copy_uvm_data((uint32_t)to, page_dir, (uint32_t)&task_args, sizeof(task_args_t));
}
int sys_execve(char *name, char **argv, char **env)
{
    task_t *task = task_current();
    kernel_strncpy(task->name, get_file_name(name), TASK_NAME_SIZE);
    // 创建新页表，销毁旧的页表
    uint32_t old_page_dir = task->tss.cr3;
    uint32_t new_page_dir = memory_create_uvm();
    if (!new_page_dir)
    {
        goto exec_failed;
    }
    // 加载elf文件到内存并完成页表映射
    uint32_t entry = load_elf_file(task, name, new_page_dir);
    if (entry == 0)
    {
        goto exec_failed;
    }
    // 为进程分配新的栈
    uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;
    int err = memory_alloc_for_page_dir(new_page_dir, MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE,
                                        MEM_TASK_STACK_SIZE, PTE_P | PTE_U | PTE_W);
    if (err < 0)
    {
        goto exec_failed;
    }

    int argc = strings_count(argv);
    err = copy_args((char *)stack_top, new_page_dir, argc, argv);
    if (err < 0)
    {
        goto exec_failed;
    }

    // 修改eip跳转到elf文件运行
    syscall_frame_t *frame = (syscall_frame_t *)(task->tss.esp0 - sizeof(syscall_frame_t));
    frame->eip = entry;

    frame->eax = frame->ebx = frame->ecx = frame->edx = 0;
    frame->esi = frame->edi = frame->ebp = 0;
    frame->eflags = EFLAGS_IF | EFLAGS_DEFAULT;
    frame->esp = stack_top - SYSCALL_PARAM_COUNT * sizeof(uint32_t);

    task->tss.cr3 = new_page_dir;
    mmu_set_page_dir(new_page_dir);
    memory_destory_uvm(old_page_dir);
    return 0;

exec_failed:
    if (new_page_dir)
    {
        task->tss.cr3 = old_page_dir;
        mmu_set_page_dir(old_page_dir);
        memory_destory_uvm(new_page_dir);
    }
    return -1;
}