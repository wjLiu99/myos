#ifndef LOADER_H
#define LOADER_H
#include "comm/boot_info.h"
#include "comm/types.h"
#include "comm/cpu_int.h"
#define SECTOR_SIZE 512
#define ELFFILE_LOADADDR (1024 * 1024)
// 声明汇编全局函数
void entry_protect_mode(void);
// 内存检测信息结构
typedef struct SMAP_entry
{
    uint32_t BaseL; // base address uint64_t
    uint32_t BaseH;
    uint32_t LengthL; // length uint64_t
    uint32_t LengthH;
    uint32_t Type; // entry Type
    uint32_t ACPI; // extended
} __attribute__((packed)) SMAP_entry_t;
extern boot_info_t boot_info;
void enable_page_mode(void);
#endif // LOADER_H
