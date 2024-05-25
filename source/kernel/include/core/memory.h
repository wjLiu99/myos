#ifndef MEMORY_H
#define MEMORY_H
#include "comm/types.h"
#include "tools/bitmap.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"

#define MEM_EXT_START (1024 * 1024)
#define MEM_PAGE_SIZE 4096
#define MEM_EBDA_START 0x80000

// 地址分配结构
typedef struct _addr_alloc_t
{
    mutex_t mutex;
    bitmap_t bitmap;    // 位图
    uint32_t start;     // 分配内存的起始地址
    uint32_t size;      // 内存大小
    uint32_t page_size; // 页大小

} addr_alloc_t;

typedef struct _memory_map_t
{
    void *vstart; // 虚拟地址
    void *vend;
    void *pstart;  // 物理地址
    uint32_t perm; // 特权

} memory_map_t;
// 内存初始化
void memory_init(boot_info_t *boot_info);
#endif