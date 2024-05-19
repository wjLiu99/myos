#include "loader.h"
#include"comm/elf.h"
// LBA方式读取磁盘
static void read_disk(uint32_t sector, uint32_t sector_cnt, uint8_t *buf)
{
    //写入扇区数高8位和LBA4-6
    outb(0x1F6, 0xE0);
    outb(0x1F2, (uint8_t)(sector_cnt >> 8));
    outb(0x1F3, (uint8_t)(sector >> 24));
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    //向端口写入扇区数低8位和LBA1-3
    outb(0x1f2, (uint8_t)(sector_cnt));
    outb(0x1f3, (uint8_t)(sector));
    outb(0x1f4, (uint8_t)(sector >> 8));
    outb(0x1f5, (uint8_t)(sector >> 16));
    //读指令
    outb(0x1f7, 0x24);
    uint16_t *data_buf = (uint16_t *)buf;
    //循环读扇区
    while (sector_cnt--)
    {
        //端口没数据到达一直循环
        while ((inb(0x1f7) & 0x88) != 0x8){}
        //inw每次读一个字，512字节需要读256次
        for(int i=0;i<SECTOR_SIZE / 2;i++){
            *data_buf++ = inw(0x1f0);
        }
        
    }
}
static uint32_t load_elf_file (uint8_t * file_buffer) {
    
    Elf32_Ehdr * elf_hdr = (Elf32_Ehdr *)file_buffer;
    if ((elf_hdr->e_ident[0] != ELF_MAGIC) || (elf_hdr->e_ident[1] != 'E')
        || (elf_hdr->e_ident[2] != 'L') || (elf_hdr->e_ident[3] != 'F')) {
        return 0;
    }

    // 然后从中加载程序头，将内容拷贝到相应的位置
    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        Elf32_Phdr * phdr = (Elf32_Phdr *)(file_buffer + elf_hdr->e_phoff) + i;
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

		// 全部使用物理地址，此时分页机制还未打开
        uint8_t * src = file_buffer + phdr->p_offset;
        uint8_t * dest = (uint8_t *)phdr->p_paddr;
        for (int j = 0; j < phdr->p_filesz; j++) {
            *dest++ = *src++;
        }

		// memsz和filesz不同时，后续要填0
		dest= (uint8_t *)phdr->p_paddr + phdr->p_filesz;
		for (int j = 0; j < phdr->p_memsz - phdr->p_filesz; j++) {
			*dest++ = 0;
		}
    }

    return elf_hdr->e_entry;
}
void die(int errno){
    while(1){};
}
void load_kernel()
{
    //读磁盘，100扇区开始读500扇区，读入到1M字节处
    read_disk(100,500,(uint8_t *)ELFFILE_LOADADDR);
    //解析elf文件将代码段和数据段加载进内存

    uint32_t kernel_load_addr = load_elf_file((uint8_t *)ELFFILE_LOADADDR);
    if(kernel_load_addr == 0){
        die(-1);
    }
    //跳转到内存1M字节处运行
    ((void (*)(boot_info_t *))kernel_load_addr)(&boot_info);
    for (;;)
    {
    }
}