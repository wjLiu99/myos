    
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