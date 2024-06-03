#include "dev/console.h"
#include "tools/kernellib.h"
#include "comm/cpu_int.h"

#define CONSOLE_NR 8
static console_t console_buf[CONSOLE_NR];

// 读取光标位置
static int read_cursor_pos(void)
{
    int pos;
    outb(0x3d4, 0xf);
    pos = inb(0x3d5);
    outb(0x3d4, 0xe);
    pos |= (inb(0x3d5) << 8);
    return pos;
}

// 写入光标位置
static int updata_cursor_pos(console_t *console)
{
    int pos = console->cursor_row * console->display_cols + console->cursor_col;
    outb(0x3d4, 0xf);
    outb(0x3d5, (uint8_t)(pos & 0xff));
    outb(0x3d4, 0xe);
    outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    return pos;
}
// 擦除
static void erase_rows(console_t *console, int start, int end)
{
    disp_char_t *disp_start = console->disp_base + console->display_cols * start;
    disp_char_t *disp_end = console->disp_base + console->display_cols * (end + 1);
    while (disp_start < disp_end)
    {
        disp_start->c = ' ';
        disp_start->foreground = console->foreground;
        disp_start->background = console->background;
        disp_start++;
    }
}

// 上滚
static void scroll_up(console_t *console, int lines)
{
    disp_char_t *dest = console->disp_base;
    disp_char_t *src = console->disp_base + lines * console->display_cols;
    uint32_t size = (console->display_rows - lines) * console->display_cols * sizeof(disp_char_t);
    kernel_memcpy(dest, src, size);
    erase_rows(console, console->display_rows - lines, console->display_rows - 1);
    console->cursor_row -= lines;
}

static void move_to_col0(console_t *console)
{
    console->cursor_col = 0;
}

static void move_next_line(console_t *console)
{

    if (++console->cursor_row >= console->display_rows)
    {
        scroll_up(console, 1);
    }
}

// 移动光标
static void move_forward(console_t *console, int size)
{
    for (int i = 0; i < size; i++)
    {

        if (++console->cursor_col >= console->display_cols)
        {
            console->cursor_row++;
            console->cursor_col = 0;

            if (console->cursor_row >= console->display_rows)
            {
                scroll_up(console, 1);
            }
        }
    }
}

static void show_char(console_t *console, char c)
{
    int offset = console->cursor_col + console->cursor_row * console->display_cols;
    disp_char_t *p = console->disp_base + offset;
    p->c = c;
    p->foreground = console->foreground;
    p->background = console->background;
    move_forward(console, 1);
}
static int move_backword(console_t *console, int n)
{
    int status = -1;
    for (int i = 0; i < n; i++)
    {
        if (console->cursor_col > 0)
        {
            console->cursor_col--;
            status = 0;
        }
        else if (console->cursor_row > 0)
        {
            console->cursor_row--;
            console->cursor_col = console->display_cols - 1;
            status = 0;
        }
    }
    return status;
}

static void erase_backword(console_t *console)
{
    if (move_backword(console, 1) == 0)
    {
        show_char(console, ' ');
        move_backword(console, 1);
    }
}
// 清空屏幕
static void clear_display(console_t *console)
{
    int size = console->display_cols * console->display_rows;
    disp_char_t *start = console->disp_base;
    for (int i = 0; i < size; i++, start++)
    {
        start->v = ' ';
        start->background = console->background;
        start->foreground = console->foreground;
    }
}

int console_init(void)
{
    for (int i = 0; i < CONSOLE_NR; i++)
    {
        console_t *console = console_buf + i;
        int cursor_pos = read_cursor_pos();
        console->display_cols = CONSOLE_COL_MAX;
        console->display_rows = CONSOLE_ROW_MAX;
        console->cursor_row = cursor_pos / console->display_cols;
        console->cursor_col = cursor_pos % console->display_cols;
        console->old_cursor_col = 0;
        console->old_cursor_row = 0;
        console->write_state = CONSOLE_WRITE_NORMAL;
        console->foreground = COLOR_White;
        console->background = COLOR_Black;
        console->disp_base = (disp_char_t *)CONSOLE_DISP_ADDR + i * (CONSOLE_ROW_MAX * CONSOLE_COL_MAX);
        // clear_display(console);
    }
    return 0;
}
// 写普通字符
static void write_normal(console_t *c, char ch)
{
    switch (ch)
    {
    case ASCII_ESC:
        c->write_state = CONSOLE_WRITE_ESC;
        break;
    case '\n':
        move_to_col0(c);
        move_next_line(c);
        break;
    case 0x7f:
        erase_backword(c);
        break;
    case '\b':
        move_backword(c, 1);
        break;
    case '\r':
        move_to_col0(c);
        break;
    default:
        if ((ch >= ' ') && (ch <= '~'))
        {
            show_char(c, ch);
            break;
        }
    }
}
// 保存光标位置
void save_cursor(console_t *console)
{
    console->old_cursor_col = console->cursor_col;
    console->old_cursor_row = console->cursor_row;
}
// 恢复光标位置
void restore_cursor(console_t *console)
{
    console->cursor_col = console->old_cursor_col;
    console->cursor_row = console->old_cursor_row;
}
static void set_font_style(console_t *console)
{
    static const color_t color_table[] = {
        COLOR_Black, COLOR_Red, COLOR_Green, COLOR_Yellow,
        COLOR_Blue, COLOR_Magenta, COLOR_Cyan, COLOR_White};
    for (int i = 0; i <= console->curr_param_index; i++)
    {
        int param = console->esc_param[i];
        if ((param >= 30) && (param <= 37))
        {
            console->foreground = color_table[param - 30];
        }
        else if ((param >= 40) && (param <= 47))
        {
            console->background = color_table[param - 40];
        }
        else if (param == 39)
        {
            console->foreground = COLOR_White;
        }
        else if (param == 49)
        {
            console->background = COLOR_Black;
        }
    }
}
static void clear_esc_param(console_t *console)
{
    kernel_memset(console->esc_param, 0, sizeof(console->esc_param));
    console->curr_param_index = 0;
}
// 写esc序列
static void write_esc(console_t *c, char ch)
{
    switch (ch)
    {
    case '7':
        save_cursor(c);
        c->write_state = CONSOLE_WRITE_NORMAL;
        break;
    case '8':
        restore_cursor(c);
        c->write_state = CONSOLE_WRITE_NORMAL;
        break;
    case '[':
        clear_esc_param(c);
        c->write_state = CONSOLE_WRITE_SQUARE;
        break;

    default:
        c->write_state = CONSOLE_WRITE_NORMAL;
        break;
    }
}
static void move_left(console_t *console, int n)
{
    if (n == 0)
    {
        n = 1;
    }
    int col = console->cursor_col - n;
    console->cursor_col = (col >= 0) ? col : 0;
}
static void move_right(console_t *console, int n)
{
    if (n == 0)
    {
        n = 1;
    }
    int col = console->cursor_col + n;
    console->cursor_col = (col >= console->display_cols) ? (console->display_cols - 1) : col;
}
static void move_cursor(console_t *console)
{
    console->cursor_row = console->esc_param[0];
    console->cursor_col = console->esc_param[1];
}
static void erase_in_display(console_t *console)
{
    if (console->curr_param_index < 0)
    {
        return;
    }
    int param = console->esc_param[0];
    if (param == 2)
    {
        erase_rows(console, 0, console->display_rows - 1);
        console->cursor_col = console->cursor_row = 0;
    }
}
// 写esc转义序列
static void write_esc_square(console_t *console, char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        int *param = &console->esc_param[console->curr_param_index];
        *param = *param * 10 + c - '0';
    }
    else if ((c == ';') && (console->curr_param_index < ESC_PARAM_MAX))
    {
        console->curr_param_index++;
    }
    else
    {
        switch (c)
        {
        case 'm':
            set_font_style(console);
            break;
        case 'D':
            move_left(console, console->esc_param[0]);
            break;
        case 'C':
            move_right(console, console->esc_param[0]);
            break;
        case 'H':
        case 'f':
            move_cursor(console);
            break;
        case 'J':
            erase_in_display(console);
            break;
        default:
            break;
        }
        console->write_state = CONSOLE_WRITE_NORMAL;
    }
}
int console_write(int console, char *data, int size)
{
    console_t *c = console_buf + console;
    int len;
    for (len = 0; len < size; len++)
    {
        char ch = *data++;
        switch (c->write_state)
        {
        case CONSOLE_WRITE_NORMAL:
            write_normal(c, ch);
            break;
        case CONSOLE_WRITE_ESC:
            write_esc(c, ch);
            break;
        case CONSOLE_WRITE_SQUARE:
            write_esc_square(c, ch);
            break;

        default:
            break;
        }
    }

    updata_cursor_pos(c);
    return len;
}

void console_close(int console)
{
}