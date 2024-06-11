#ifndef DEV_H
#define DEV_H
#define DEV_NAME_SIZE 32
struct _dev_desc_t;
enum
{
    DEV_UNKNOWN = 0,
    DEV_TTY,
    DEV_DISK,
};
// 某一特定设备
typedef struct _device_t
{
    struct _dev_desc_t *desc;
    int mode;  // 只读
    int minor; // 次设备号
    void *data;
    int open_count; // 打开次数
} device_t;

// 某一类型的设备
typedef struct _dev_desc_t
{
    char name[DEV_NAME_SIZE];
    int major; // 主设备号

    int (*open)(device_t *dev);
    int (*read)(device_t *dev, int addr, char *buf, int size);
    int (*write)(device_t *dev, int addr, char *buf, int size);
    int (*control)(device_t *dev, int cnd, int arg0, int arg1);
    void (*close)(device_t *dev);
} dev_desc_t;

int dev_open(int major, int minor, void *data);
int dev_read(int dev_id, int addr, char *buf, int size);
int dev_write(int dev_id, int addr, char *buf, int size);
int dev_control(int dev_id, int cmd, int arg0, int arg1);
void dev_close(int dev_id);
#endif