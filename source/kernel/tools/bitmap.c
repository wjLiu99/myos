#include "tools/bitmap.h"
#include "tools/kernellib.h"

int bitmap_byte_count(int bit_count)
{
    return (bit_count + 7) / 8;
}

void bitmap_init(bitmap_t *bitmap, uint8_t *bits, int count, int init_bit)
{
    bitmap->bit_count = count;
    bitmap->bits = bits;
    int bytes = bitmap_byte_count(bitmap->bit_count);
    kernel_memset(bitmap->bits, init_bit ? 0xff : 0, bytes);
}

int bitmap_get_bit(bitmap_t *bitmap, int index)
{
    return bitmap->bits[index / 8] & (1 << (index % 8));
}

void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int bit)
{
    for (int i = 0; (i < count) && (index < bitmap->bit_count); i++, index++)
    {
        if (bit)
        {
            bitmap->bits[index / 8] |= (1 << (index % 8));
        }
        else
        {
            bitmap->bits[index / 8] &= ~(1 << (index % 8));
        }
    }
}

int bitmap_is_set(bitmap_t *bitmap, int index)
{
    return bitmap_get_bit(bitmap, index) ? 1 : 0;
}

int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count)
{
    int search_index = 0;
    int find_index = -1;
    while (search_index < bitmap->bit_count)
    {
        if (bitmap_get_bit(bitmap, search_index) != bit)
        {
            search_index++;
            continue;
        }
        find_index = search_index;
        int i;
        for (i = 1; (i < count) && (search_index < bitmap->bit_count); i++)
        {
            if (bitmap_get_bit(bitmap, search_index++) != bit)
            {
                find_index = -1;
                break;
            }
        }
        if (i >= count)
        {

            bitmap_set_bit(bitmap, find_index, count, ~bit);
            return find_index;
        }
    }
}
