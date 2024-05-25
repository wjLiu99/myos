#ifndef BITMAP_H
#define BITMAP_H
#include "comm/types.h"

typedef struct _bitmap_t
{
    int bit_count;
    uint8_t *bits;
} bitmap_t;

void bitmap_init(bitmap_t *bitmap, uint8_t *bits, int count, int init_bit);
int bitmap_byte_count(int bit_count);
int bitmap_get_bit(bitmap_t *bitmap, int index);                      // 查看某一位的值
void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int bit); // 设置连续count位的值为bit
int bitmap_is_set(bitmap_t *bitmap, int index);                       // 查看某一位是否为1
int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count);         // 将位图连续的count个为bit的位分配出去
#endif