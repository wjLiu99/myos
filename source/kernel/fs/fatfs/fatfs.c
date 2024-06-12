#include "fs/fatfs/fatfs.h"
#include "fs/fs.h"
#include "dev/dev.h"
#include "tools/kernellib.h"
#include "tools/log.h"
#include "core/memory.h"

file_type_t diritem_get_type(diritem_t *diritem)
{

    if ((diritem->DIR_Attr & (DIRITEM_ATTR_VOLUME_ID | DIRITEM_ATTR_HIDDEN | DIRITEM_ATTR_SYSTEM)))
    {
        return FILE_UNKNOWN;
    }
    if ((diritem->DIR_Attr & DIRITEM_ATTR_LONG_NAME) == DIRITEM_ATTR_LONG_NAME)
    {
        return FILE_UNKNOWN;
    }
    return diritem->DIR_Attr & DIRITEM_ATTR_DIRECTORY ? FILE_DIR : FILE_NORMAL;
}

static int bread_sector(fat_t *fat, int sector)
{
    if (sector = fat->curr_sector)
    {
        return 0;
    }
    int cnt = dev_read(fat->fs->dev_id, sector, fat->fat_buffer, 1);
    if (cnt == 1)
    {
        fat->curr_sector = sector;
        return 0;
    }
    return -1;
}
static diritem_t *read_dir_entry(fat_t *fat, int index)
{
    if ((index < 0) || (index >= fat->root_ent_cnt))
    {
        return (diritem_t *)0;
    }

    int offset = index * sizeof(diritem_t);
    int sector = fat->root_start + offset / fat->bytes_per_sec;
    int err = bread_sector(fat, sector);
    if (err < 0)
    {
        return (diritem_t *)0;
    }
    return (diritem_t *)(fat->fat_buffer + offset % fat->bytes_per_sec);
}

void diritem_get_name(diritem_t *item, char *dest)
{
    char *c = dest;
    char *ext = (char *)0;

    kernel_memset(dest, 0, 12);
    for (int i = 0; i < 11; i++)
    {
        if (item->DIR_Name[i] != ' ')
        {
            *c++ = item->DIR_Name[i];
        }

        if (i == 7)
        {
            ext = c;
            *c++ = '.';
        }
    }

    if (ext && (ext[1] == '\0'))
    {
        ext[0] = '\0';
    }
}
int fatfs_mount(struct _fs_t *fs, int major, int minor)
{
    int dev_id = dev_open(major, minor, (void *)0);
    if (dev_id < 0)
    {
        log_printf("open disk failed . major :%x , minor :%x", major, minor);
        return -1;
    }

    // 在内存中分配一页保存读取的数据
    dbr_t *dbr = (dbr_t *)memory_alloc_page();
    if (!dbr)
    {
        log_printf("mount failed . can't alloc buf");
        goto mount_failed;
    }
    // 读一个扇区，0是相对分区的扇区号
    int cnt = dev_read(dev_id, 0, (char *)dbr, 1);
    if (cnt < 1)
    {
        log_printf("read dbr failed");
        goto mount_failed;
    }
    fat_t *fat = &fs->fat_data;

    fat->fat_buffer = (uint8_t *)dbr;
    fat->bytes_per_sec = dbr->BPB_BytsPerSec;
    fat->tbl_start = dbr->BPB_RsvdSecCnt;
    fat->tbl_sectors = dbr->BPB_FATSz16;
    fat->tbl_cnt = dbr->BPB_NumFATs;
    fat->root_ent_cnt = dbr->BPB_RootEntCnt;
    fat->sec_per_cluster = dbr->BPB_SecPerClus;
    fat->cluster_byte_size = fat->sec_per_cluster * dbr->BPB_BytsPerSec;
    fat->root_start = fat->tbl_start + fat->tbl_sectors * fat->tbl_cnt;
    fat->data_start = fat->root_start + fat->root_ent_cnt * 32 / SECTOR_SIZE;
    fat->curr_sector = -1;
    fat->fs = fs;
    mutex_init(&fat->mutex);
    fs->mutex = &fat->mutex;
    if (fat->tbl_cnt != 2)
    {
        log_printf("fat table num error, major: %x, minor: %x", major, minor);
        goto mount_failed;
    }

    if (kernel_memcmp(dbr->BS_FileSysType, "FAT16", 5) != 0)
    {
        log_printf("not a fat16 file system, major: %x, minor: %x", major, minor);
        goto mount_failed;
    }

    // 记录相关的打开信息
    fs->type = FS_FAT16;
    fs->data = &fs->fat_data;
    fs->dev_id = dev_id;
    return 0;

mount_failed:
    if (dbr)
    {
        memory_free_page((uint32_t)dbr);
    }
    dev_close(dev_id);
    return -1;
}

void fatfs_unmount(struct _fs_t *fs)
{
    fat_t *fat = (fat_t *)fs->data;
    dev_close(fs->dev_id);
    memory_free_page((uint32_t)fat->fat_buffer);
}
static void to_sfn(char *dest, const char *src)
{
    kernel_memset(dest, ' ', 11);
    char *curr = dest;
    char *end = dest + 11;
    while (*src && (curr < end))
    {
        char c = *src++;
        switch (c)
        {
        case '.':
            curr = dest + 8;
            break;

        default:
            if ((c >= 'a') && (c <= 'z'))
            {
                c = c - 'a' + 'A';
            }
            *curr++ = c;
            break;
        }
    }
}

int diritem_name_match(diritem_t *item, const char *path)
{
    char buf[11];
    // short file name
    to_sfn(buf, path);
    return kernel_memcmp(buf, item->DIR_Name, 11) == 0;
}

static void read_from_diritem(fat_t *fat, file_t *file, diritem_t *item, int index)
{
    file->type = diritem_get_type(item);
    file->size = item->DIR_FileSize;
    file->pos = 0;
    file->p_index = index;
    file->sblk = (item->DIR_FstClusHI << 16) | (item->DIR_FstClusL0);
    file->cblk = file->sblk;
}
int fatfs_open(struct _fs_t *fs, const char *path, file_t *file)
{
    fat_t *fat = (fat_t *)fs->data;
    diritem_t *file_item = (diritem_t *)0;
    int p_index = -1;
    for (int i = 0; i < fat->root_ent_cnt; i++)
    {
        diritem_t *item = read_dir_entry(fat, i);
        if (item == (diritem_t *)0)
        {
            return -1;
        }

        if (item->DIR_Name[0] == DIRITEM_NAME_END)
        {
            break;
        }

        if (item->DIR_Name[0] == DIRITEM_NAME_FREE)
        {
            continue;
        }

        if (diritem_name_match(item, path))
        {
            file_item = item;
            break;
        }
    }
    if (file_item)
    {
        read_from_diritem(fat, file, file_item, p_index);
        return 0;
    }

    return -1;
}

int fatfs_read(char *buf, int size, file_t *file)
{
    return dev_read(file->dev_id, file->pos, buf, size);
}

int fatfs_write(char *buf, int size, file_t *file)
{
    return dev_write(file->dev_id, file->pos, buf, size);
}

void fatfs_close(file_t *file)
{
    dev_close(file->dev_id);
}

int fatfs_seek(file_t *file, uint32_t offset, int dir)
{
    return -1;
}
int fatfs_stat(file_t *file, struct stat *st)
{
    return -1;
}

int fatfs_opendir(struct _fs_t *fs, const char *name, DIR *dir)
{
    dir->index = 0;
    return 0;
}
int fatfs_readdir(struct _fs_t *fs, DIR *dir, struct dirent *dirent)
{
    fat_t *fat = (fat_t *)fs->data;
    while (dir->index < fat->root_ent_cnt)
    {
        diritem_t *item = read_dir_entry(fat, dir->index);
        if (item == (diritem_t *)0)
        {
            return -1;
        }

        if (item->DIR_Name[0] == DIRITEM_NAME_END)
        {
            break;
        }
        if (item->DIR_Name[0] != DIRITEM_NAME_FREE)
        {
            file_type_t type = diritem_get_type(item);
            if ((type == FILE_NORMAL) || (type == FILE_DIR))
            {
                dirent->size = item->DIR_FileSize;
                dirent->type = type;
                diritem_get_name(item, dirent->name);
                dirent->index = dirent->index++;
                return 0;
            }
        }
        dir->index++;
    }
    return -1;
}
int fatfs_closedir(struct _fs_t *fs, DIR *dir)
{
    return 0;
}

fs_op_t fatfs_op = {
    .mount = fatfs_mount,
    .unmount = fatfs_unmount,
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .close = fatfs_close,
    .seek = fatfs_seek,
    .stat = fatfs_stat,

    .opendir = fatfs_opendir,
    .readdir = fatfs_readdir,
    .closedir = fatfs_closedir,
};