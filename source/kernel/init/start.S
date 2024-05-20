    #include "os_conf.h"
    
    .text
    .extern kernel_init
    .extern init_main
    .global _start
_start:
    push %ebp
    mov %esp ,%ebp
    mov 0x8(%ebp),%eax
    push %eax
    call kernel_init
    jmp $CS_SELECTOR, $gdt_reload

gdt_reload:
	mov $DS_SELECTOR, %ax		// 16为数据段选择子
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov $(stack + KERNEL_STACK_SIZE), %esp //设置栈
    jmp init_main

    .bss
.comm stack , KERNEL_STACK_SIZE


.macro exception_handler name num err_code //定义宏重用代码 参数：错误名，错误号，压栈的错误码
    .text
    .extern do_handler_\name
    .global exception_handler_\name
exception_handler_\name:
    .if \err_code == 0
        push $0
    .endif

    push $\num
    //保存现场
    pusha
	push %ds
	push %es
	push %fs
	push %gs
    push %esp
    call do_handler_\name
    add $(1*4), %esp //平栈

    //返回前准备
    pop %gs
	pop %fs
	pop %es
	pop %ds
	popa
    add $(2*4), %esp //平栈
    iret //中断返回,只会弹出eip,cs,eflags
.endm

//中断处理函数
exception_handler unknown, -1, 0
exception_handler divider, 0, 0
exception_handler Debug, 1, 0
exception_handler NMI, 2, 0
exception_handler breakpoint, 3, 0
exception_handler overflow, 4, 0
exception_handler bound_range, 5, 0
exception_handler invalid_opcode, 6, 0
exception_handler device_unavailable, 7, 0
exception_handler double_fault, 8, 1
exception_handler invalid_tss, 10, 1
exception_handler segment_not_present, 11, 1
exception_handler stack_segment_fault, 12, 1
exception_handler general_protection, 13, 1
exception_handler page_fault, 14, 1
exception_handler fpu_error, 16, 0
exception_handler alignment_check, 17, 1
exception_handler machine_check, 18, 0
exception_handler smd_exception, 19, 0
exception_handler virtual_exception, 20, 0

exception_handler time,0x20,0