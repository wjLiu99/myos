#include "fs/fs.h"
#include "tools/kernellib.h"
#include "comm/types.h"
#include "comm/cpu_int.h"
#include "comm/boot_info.h"
#include "dev/console.h"
#include "fs/file.h"
#include "core/task.h"
#include "dev/dev.h"
#include <tools/log.h>
#include <sys/file.h>
#include "dev/disk.h"

#define FS_TABLE_SIZE 10
static list_t mounted_list;
static fs_t fs_table[FS_TABLE_SIZE];
static list_t free_list;

extern fs_op_t devfs_op;
int path_to_num(const char *path, int *num)
{
    int n = 0;
    const char *c = path;
    while (*c)
    {
        n = n * 10 + *c - '0';
        c++;
    }
    *num = n;
    return 0;
}
const char *path_next_child(const char *path)
{
    const char *c = path;
    while (*c && (*c++ == '/'))
    {
    }
    while (*c && (*c++ != '/'))
    {
    }
    return *c ? c : (const char *)0;
}
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

static int is_fd_bad(int file)
{
    if ((file < 0) || (file >= TASK_OFILE_NR))
    {
        return 1;
    }
    return 0;
}
static int is_path_vaild(const char *path)
{
    if ((path == (const char *)0) || (path[0] == '\0'))
    {
        return 0;
    }
    return 1;
}
static void fs_protect(fs_t *fs)
{
    if (fs->mutex)
    {
        mutex_lock(fs->mutex);
    }
}
static void fs_unprotect(fs_t *fs)
{
    if (fs->mutex)
    {
        mutex_unlock(fs->mutex);
    }
}
int path_begin_with(const char *path, const char *str)
{
    const char *s1 = path, *s2 = str;
    while (*s1 && *s2 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s2 == '\0';
}
int sys_open(const char *name, int flags, ...)
{

    if (kernel_strncmp(name, "/shell.elf", 3) == 0)
    {
        read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
        temp_pos = (uint8_t *)TEMP_ADDR;
        return TEMP_FILE_ID;
    }

    file_t *file = file_alloc();
    if (!file)
    {
        return -1;
    }

    // 分配进程文件描述符
    int fd = task_alloc_fd(file);
    if (fd < 0)
    {
        goto sys_open_failed;
    }
    fs_t *fs = (fs_t *)0;
    list_node_t *node = list_first(&mounted_list);
    while (node)
    {
        fs_t *curr = list_node_parent(node, fs_t, node);
        if (path_begin_with(name, curr->mount_point))
        {
            fs = curr;
            break;
        }
        node = list_node_next(node);
    }
    if (fs)
    {
        name = path_next_child(name);
    }
    else
    {
    }

    file->mode = flags;
    file->fs = fs;
    kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);
    fs_protect(fs);
    int err = fs->op->open(fs, name, file);
    if (file < 0)
    {
        fs_unprotect(fs);
        log_printf("open %s failed", name);
        goto sys_open_failed;
    }
    fs_unprotect(fs);
    return fd;
sys_open_failed:

    file_free(file);
    if (fd >= 0)
    {
        task_remove_fd(fd);
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
    if (is_fd_bad(file) || !ptr || !len)
    {

        return 0;
    }

    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }

    if (p_file->mode == O_WRONLY)
    {
        log_printf("file is write only");
        return -1;
    }

    fs_t *fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->read(ptr, len, p_file);
    fs_unprotect(fs);
    return err;
}
static void mount_list_init(void)
{
    list_init(&free_list);
    for (int i = 0; i < FS_TABLE_SIZE; i++)
    {
        list_insert_first(&free_list, &fs_table[i].node);
    }
    list_init(&mounted_list);
}

static fs_op_t *get_fs_op(fs_type_t type, int major)
{
    switch (type)
    {
    case FS_DEVFS:
        return &(devfs_op);

    default:
        return (fs_op_t *)0;
    }
}
static fs_t *mount(fs_type_t type, char *mount_point, int dev_major, int dev_minor)
{
    fs_t *fs = (fs_t *)0;
    log_printf("mount file system ,name :%s ,dev : %x", mount_point, dev_major);
    list_node_t *curr = list_first(&mounted_list);
    while (curr)
    {
        fs_t *fs = list_node_parent(curr, fs_t, node);
        if (kernel_strncmp(fs->mount_point, mount_point, FS_MOUNTPN_SIZE) == 0)
        {
            log_printf("fs already mounted");
            goto mount_failed;
        }
        curr = list_node_next(curr);
    }

    list_node_t *free_node = list_remove_first(&free_list);
    if (!free_node)
    {
        log_printf("no free fs");
        goto mount_failed;
    }

    fs = list_node_parent(free_node, fs_t, node);
    fs_op_t *op = get_fs_op(type, dev_major);
    if (!op)
    {
        log_printf("unsupported fs type: %d", type);
    }
    kernel_memset(fs, 0, sizeof(fs_t));
    kernel_strncpy(fs->mount_point, mount_point, FS_MOUNTPN_SIZE);
    fs->op = op;
    if (op->mount(fs, dev_major, dev_minor) < 0)
    {
        log_printf("mount fs %s failed", mount_point);
        goto mount_failed;
    }
    list_insert_last(&mounted_list, &fs->node);
    return fs;

mount_failed:
    if (fs)
    {
        list_insert_last(&free_list, &fs->node);
    }
}
void fs_init(void)
{
    mount_list_init();
    file_table_init();

    disk_init();
    fs_t *fs = mount(FS_DEVFS, "/dev", 0, 0);
    ASSERT(fs != (fs_t *)0);
}
int sys_write(int file, char *ptr, int len)
{

    if (is_fd_bad(file) || !ptr || !len)
    {

        return 0;
    }

    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }

    if (p_file->mode == O_RDONLY)
    {
        log_printf("file is write only");
        return -1;
    }

    fs_t *fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->write(ptr, len, p_file);
    fs_unprotect(fs);
    return err;
}
int sys_lseek(int file, int ptr, int dir)
{
    if (file == TEMP_FILE_ID)
    {
        temp_pos = (uint8_t *)(TEMP_ADDR + ptr);
        return 0;
    }
    if (is_fd_bad(file))
    {

        return 0;
    }

    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }

    fs_t *fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->seek(p_file, ptr, dir);
    fs_unprotect(fs);
    return err;
}
int sys_close(int file)
{
    if (file == TEMP_FILE_ID)
    {
        return 0;
    }
    if (is_fd_bad(file))
    {
        log_printf("file err");
        return 0;
    }
    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }
    ASSERT(p_file->ref > 0);

    if (p_file->ref-- == 1)
    {
        fs_t *fs = p_file->fs;
        fs_protect(fs);
        fs->op->close(p_file);
        fs_unprotect(fs);
    }
    task_remove_fd(file);

    return 0;
}

int sys_isatty(int file)
{
    if (is_fd_bad(file))
    {
        log_printf("file err");
        return 0;
    }
    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }
    return p_file->type == FILE_TTY;
}

int sys_fstat(int file, struct stat *st)
{
    if (is_fd_bad(file))
    {
        log_printf("file err");
        return 0;
    }
    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not opened");
        return -1;
    }
    kernel_memset(st, 0, sizeof(struct stat));
    fs_t *fs = p_file->fs;

    fs_protect(fs);
    int err = fs->op->stat(p_file, st);
    fs_unprotect(fs);
    return err;
}
int sys_dup(int file)
{
    if (is_fd_bad(file))
    {
        log_printf("fd is not waild");
        return -1;
    }
    file_t *p_file = task_file(file);
    if (!p_file)
    {
        log_printf("file not open");
        return -1;
    }
    int fd = task_alloc_fd(p_file);
    if (fd > 0)
    {
        file_inc_ref(p_file);

        return fd;
    }
    log_printf("not file ");
    return -1;
}