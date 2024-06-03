#include "fs/file.h"
#include "ipc/mutex.h"
#include "tools/kernellib.h"
#define FILE_TABLE_SIZE 2048
static mutex_t file_alloc_mutex;
static file_t file_table[FILE_TABLE_SIZE];

file_t *file_alloc(void)
{
    file_t *file = (file_t *)0;
    mutex_lock(&file_alloc_mutex);
    for (int i = 0; i < FILE_TABLE_SIZE; i++)
    {
        file_t *p_file = file_table + i;
        if (p_file->ref == 0)
        {
            kernel_memset(p_file, 0, sizeof(file_t));
            p_file->ref = 1;
            file = p_file;
            break;
        }
    }
    mutex_unlock(&file_alloc_mutex);
    return file;
}
void file_free(file_t *file)
{
}
void file_table_init(void)
{
    mutex_init(&file_alloc_mutex);
    kernel_memset(file_table, 0, sizeof(file_table));
}