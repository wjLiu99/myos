#include "fs/fs.h"
#include "tools/kernellib.h"
#include "comm/types.h"
#include "comm/cpu_int.h"
#include "comm/boot_info.h"
#include "dev/console.h"
static void read_disk(uint32_t sector, uint32_t sector_cnt, uint8_t *buf)
{
    // 写入扇区数高8位和LBA4-6
    outb(0x1F6, 0xE0);
    outb(0x1F2, (uint8_t)(sector_cnt >> 8));
    outb(0x1F3, (uint8_t)(sector >> 24));
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    // 向端口写入扇区数低8位和LBA1-3
    outb(0x1f2, (uint8_t)(sector_cnt));
    outb(0x1f3, (uint8_t)(sector));
    outb(0x1f4, (uint8_t)(sector >> 8));
    outb(0x1f5, (uint8_t)(sector >> 16));
    // 读指令
    outb(0x1f7, 0x24);
    uint16_t *data_buf = (uint16_t *)buf;
    // 循环读扇区
    while (sector_cnt--)
    {
        // 端口没数据到达一直循环
        while ((inb(0x1f7) & 0x88) != 0x8)
        {
        }
        // inw每次读一个字，512字节需要读256次
        for (int i = 0; i < 512 / 2; i++)
        {
            *data_buf++ = inw(0x1f0);
        }
    }
}
static uint8_t TEMP_ADDR[100 * 1024];
static uint8_t *temp_pos;
#define TEMP_FILE_ID 100
int sys_open(const char *name, int flags, ...)
{
    if (name[0] == '/')
    {
        read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
        temp_pos = (uint8_t *)TEMP_ADDR;
        return TEMP_FILE_ID;
    }
    return -1;
}
int sys_read(int file, char *ptr, int len)
{
    if (file == TEMP_FILE_ID)
    {
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }
    return -1;
}
#include <tools/log.h>

int sys_write(int file, char *ptr, int len)
{
    // ptr[len] = '\0';
    // log_printf("%s", ptr);
    console_write(0, ptr, len);
    return -1;
}
int sys_lseek(int file, int ptr, int dir)
{
    if (file == TEMP_FILE_ID)
    {
        temp_pos = (uint8_t *)(TEMP_ADDR + ptr);
        return 0;
    }
}
int sys_close(int file)
{
    return 0;
}

int sys_isatty(int file)
{
    return -1;
}

int sys_fstat(int file, struct stat *st)
{
    return -1;
}