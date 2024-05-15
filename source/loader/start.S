  	// 16位代码
  	.code16
 	.text
 	.extern loader_entry
	.global _start
_start:
	


    jmp loader_entry


	.code32
	.text	
	.global entry_protect_mode
	.extern load_kernel
entry_protect_mode:
	mov $16 ,%ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	jmp $8,$load_kernel



