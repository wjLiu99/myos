#include "dev/tty.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "dev/kbd.h"
#include "dev/console.h"
#include "cpu/irq.h"

#define TTY_NR 8
static tty_t tty_devs[TTY_NR];

static void tty_fifo_init(tty_fifo_t *fifo, char *buf, int size)
{
    fifo->buf = buf;
    fifo->count = 0;
    fifo->read = fifo->write = 0;
    fifo->size = size;
}
int tty_fifo_put(tty_fifo_t *fifo, char c)
{
    irq_state_t state = irq_enter_protection();
    if (fifo->count > fifo->size)
    {
        irq_leave_protection(state);

        return -1;
    }
    fifo->buf[fifo->write++] = c;
    if (fifo->write >= fifo->size)
    {
        fifo->write = 0;
    }
    fifo->count++;
    irq_leave_protection(state);
    return 0;
}

int tty_fifo_get(tty_fifo_t *fifo, char *c)
{
    irq_state_t state = irq_enter_protection();
    if (fifo->count <= 0)
    {
        irq_leave_protection(state);
        return -1;
    }
    *c = fifo->buf[fifo->read++];
    if (fifo->read >= fifo->size)
    {
        fifo->read = 0;
    }
    fifo->count--;
    irq_leave_protection(state);
    return 0;
}
static tty_t *get_tty(device_t *dev)
{
    int idx = dev->minor;
    if ((idx < 0) || (idx >= TTY_NR) || (!dev->open_count))
    {
        log_printf("tty is not open");
        return (tty_t *)0;
    }
    return tty_devs + idx;
}
int tty_open(device_t *dev)
{
    int idx = dev->minor;
    if ((idx < 0) || (idx >= TTY_NR))
    {
        log_printf("open tty failed");
        return -1;
    }
    tty_t *tty = tty_devs + idx;
    sem_init(&tty->osem, TTY_OBUF_SIZE);
    sem_init(&tty->isem, 0);
    tty_fifo_init(&tty->ififo, tty->ibuf, TTY_IBUF_SIZE);
    tty_fifo_init(&tty->ofifo, tty->obuf, TTY_OBUF_SIZE);
    tty->oflags = TTY_OCRLF;
    tty->iflags = TTY_IECHO | TTY_INCLR;
    tty->console_idx = idx;
    kbd_init();
    console_init(idx);
    return 0;
}

int tty_write(device_t *dev, int addr, char *buf, int size)
{
    if (size < 0)
    {
        return -1;
    }
    tty_t *tty = get_tty(dev);
    if (!tty)
    {
        return -1;
    }
    int len = 0;
    while (size)
    {
        char c = *buf++;

        if ((c == '\n') && (tty->oflags & TTY_OCRLF))
        {
            sem_wait(&tty->osem);
            int err = tty_fifo_put(&tty->ofifo, '\r');
            if (err < 0)
            {
                break;
            }
        }

        sem_wait(&tty->osem);
        int err = tty_fifo_put(&tty->ofifo, c);
        if (err < 0)
        {
            break;
        }
        len++;
        size--;

        console_write(tty);
    }

    return size;
}

int tty_read(device_t *dev, int addr, char *buf, int size)
{
    if (size < 0)
    {
        return -1;
    }
    tty_t *tty = get_tty(dev);
    char *pbuf = buf;
    int len = 0;
    while (len < size)
    {
        sem_wait(&tty->isem);
        char ch;
        tty_fifo_get(&tty->ififo, &ch);
        switch (ch)
        {
        case 0x7f:
            if (len == 0)
            {
                continue;
            }
            len--;
            pbuf--;
            break;
        case '\n':
            if ((tty->iflags & TTY_INCLR) && (len < size - 1))
            {
                *pbuf++ = '\r';
                len++;
            }
            *pbuf++ = '\n';
            len++;
            break;

        default:
            *pbuf++ = ch;
            len++;
            break;
        }
        if (tty->iflags & TTY_IECHO)
        {
            tty_write(dev, 0, &ch, 1);
        }
        if ((ch == '\n') || (ch == '\r'))
        {
            break;
        }
    }
    // size 不行
    return len;
}
int tty_control(device_t *dev, int cnd, int arg0, int arg1)
{
    return 0;
}
void tty_close(device_t *dev)
{
    return;
}

dev_desc_t dev_tty_desc = {
    .name = "tty",
    .major = DEV_TTY,
    .open = tty_open,
    .close = tty_close,
    .control = tty_control,
    .read = tty_read,
    .write = tty_write,
};

static int curr_tty = 0;
void tty_in(char ch)
{
    tty_t *tty = tty_devs + curr_tty;
    if (sem_count(&tty->isem) >= TTY_IBUF_SIZE)
    {
        return;
    }
    tty_fifo_put(&tty->ififo, ch);
    sem_wakeup(&tty->isem);
}

void tty_select(int tty)
{
    if (tty != curr_tty)
    {
        console_select(tty);
        curr_tty = tty;
    }
}