#include "loader.h"

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
void load_kernel()
{
    //读磁盘，100扇区开始读500扇区，读入到1M字节处
    read_disk(100,500,(uint8_t *)KERNEL_LOADADDR);
    //跳转到内存1M字节处运行
    ((void (*)(boot_info_t *))KERNEL_LOADADDR)(&boot_info);
    for (;;)
    {
    }
}