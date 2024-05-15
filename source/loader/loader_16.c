__asm__(".code16gcc");
#include "loader.h"
boot_info_t boot_info;
static void show_msg(const char *msg){
	char c;
	while (((c = *msg++) != '\0'))
	{
		asm(
			"mov $0xe, %%ah\n\t"
			"mov %[ch], %%al\n\t"
			"int $0x10"::[ch]"r"(c)
		);

	}
	
}
//GDT表
uint16_t gdt_table[][4]={
	{0,0,0,0},
	{0xFFFF,0x0000,0x9a00,0x00cf},
	{0xFFFF,0x0000,0x9200,0x00cf},
};

static void  detect_memory(void) {
	uint32_t contID = 0;
	SMAP_entry_t smap_entry;
	int signature, bytes;

    show_msg("try to detect memory:");

	// 初次：EDX=0x534D4150,EAX=0xE820,ECX=24,INT 0x15, EBX=0（初次）
	// 后续：EAX=0xE820,ECX=24,
	// 结束判断：EBX=0
	boot_info.ram_region_count = 0;
	for (int i = 0; i < BOOT_RAM_REGION_MAX; i++) {
		SMAP_entry_t * entry = &smap_entry;

		__asm__ __volatile__("int  $0x15"
			: "=a"(signature), "=c"(bytes), "=b"(contID)
			: "a"(0xE820), "b"(contID), "c"(24), "d"(0x534D4150), "D"(entry));
		if (signature != 0x534D4150) {
            show_msg("failed.\r\n");
			return;
		}

		// todo: 20字节
		if (bytes > 20 && (entry->ACPI & 0x0001) == 0){
			continue;
		}

        // 保存RAM信息，只取32位，空间有限无需考虑更大容量的情况
        if (entry->Type == 1) {
            boot_info.ram_region_cfg[boot_info.ram_region_count].start = entry->BaseL;
            boot_info.ram_region_cfg[boot_info.ram_region_count].size = entry->LengthL;
            boot_info.ram_region_count++;
        }

		if (contID == 0) {
			break;
		}
	}
    show_msg("ok.\r\n");
}

static void enter_protect_m(void){ //进入保护模式
	//关中断
	cli();
	//打开A20地址线
	uint8_t ret = inb(0x92);
	outb(0x92,ret | 0x2);

	//加载GDT表
	lgdt((uint32_t)gdt_table , sizeof(gdt_table));


}
void loader_entry(void){
	show_msg("......loading.....\n\r");
	detect_memory();
	enter_protect_m();
    for(;;) {}
}


