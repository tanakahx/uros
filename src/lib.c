#include "uros.h"
#include "uart.h"
#include "uart_hal.h"

#define HEAP_SIZE 4096 /* KB */
#define HEAP_UNIT_SIZE sizeof(cell_t)

typedef struct cell {
    struct cell *next;
    size_t size;
} cell_t;

static cell_t heap[HEAP_SIZE/HEAP_UNIT_SIZE + 1]; /* includes sentinel */
static cell_t *freep;

void *memset(void *b, int c, size_t len)
{
    char *p = (char *)b;

    while (len--)
        *p++ = c;
    return b;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    char *p1 = (char *)dst;
    const char *p2 = (const char *)src;

    while (n--)
        *p1++ = *p2++;
    return dst;
}

/* Memory allocation algorithm is near fit */
void *mem_alloc(size_t size)
{
    cell_t *p; /* current pointer */
    cell_t *q; /* previous pointer */
    size_t unit = (size + HEAP_UNIT_SIZE - 1) / HEAP_UNIT_SIZE + 1; /* includes header */

    if (freep == NULL) {
        /* heap[0] is used as a sentinel */
        heap[0].next = &heap[1];
        heap[0].size = 0;

        /* heap[1] contains the whole heap area at first */
        heap[1].next = &heap[0];
        heap[1].size = HEAP_SIZE;

        freep = heap;
    }

    for (q = freep, p = freep->next; p != freep; q = p, p = p->next) {
        if (unit <= p->size) {
            if (unit == p->size) {
                /* remove a whole block from free list */
                q->next = p->next;
            }
            else {
                /* allocate from the tail of a block */
                p->size -= unit;
                p += p->size;
                p->size = unit;
            }
            /* remember the pointer to be released */
            freep = q;
            return (void *)(p+1); 
        }
    }

    /* no more heap memory */
    return NULL;
}

void mem_free(void *addr)
{
    cell_t *p;
    cell_t *t;

    if (addr == NULL)
        return;

    t = (cell_t *)addr - 1;

    /*
     * The freeing cell pointed by t is inserted before a cell pointed by p.
     * There are two cases, an edge case and others.
     * case 1(others): [p  (>heap)] => [t] => [p->next (>heap)]
     * case 2(edge)  : [p (>=heap)] => [t] => [p->next (=heap)]
     * 
     * The position where the cell t is inserted is searched from freep, 
     * which is pointing the free block that has allocated a cell most recently, 
     * because a recentlly allocated cell is expected to be released immediately
     * and this reduces the time to search position to insert the freeing cell.
     */
    for (p = freep; ; p = p->next) {
        if ((p < p->next && t < p->next) || p >= p->next) {
            break;
        }
    }

    /* compaction t and the next cell */
    if (t + t->size == p->next) {
        t->size += p->next->size;
        t->next  = p->next->next;
    }
    else {
        t->next = p->next;
    }

    /* compaction t and the previous cell */
    if (p + p->size == t) {
        p->size += t->size; 
        p->next  = t->next;
    }
    else {
        p->next = t;
    }
}

size_t strlen(const char *s)
{
    const char *p = s;
    
    while (*p)
        p++;

    return p - s;
}

void uart_put_str(char *s, size_t size)
{
    uart_info_t info;
    uint32_t devno;

#ifdef LM3S6965EVB
    devno = 0;
#elif  STM32F407xx
    devno = 1;
#endif
    info.devno = devno;
    info.baud_rate = 115200;

    uart_open(&info);
    uart_write(devno, s, size);
    /* uart_close(&info); */
}

int uart_get_str(char *s, size_t size)
{
    uart_info_t info;
    uint32_t devno;
    int ret;

#ifdef LM3S6965EVB
    devno = 0;
#elif  STM32F407xx
    devno = 1;
#endif
    info.devno = devno;
    info.baud_rate = 115200;

    uart_open(&info);
    if ((ret = uart_tread(devno, s, size, 40)) == -1)
        *s = '.';
    /* uart_close(&info); */
    return ret;
}

void putc(char c)
{
    uart_put_str(&c, 1);
}

void putchar(char c)
{
    putc(c);
    if (c == '\n')
        putc('\r');
}

void puts(const char *s)
{
    while (*s)
        putchar(*s++);
    putchar('\n');
}

void putdec(unsigned int n)
{
    if (n < 10)
        putchar("0123456789"[n]);
    else {
        putdec(n / 10);
        putdec(n % 10);
    }
}

void puthex(unsigned int n)
{
    if (n < 16)
        putchar("0123456789ABCDEF"[n]);
    else {
        puthex(n >> 4);
        puthex(n & 0xF);
    }
}

void puthex_n(unsigned int n, int column)
{
    unsigned int d = n;
    int zlen = column - 1;

    while (d >= 16) {
        d >>= 4;
        zlen--;
    }
    while (zlen-- > 0)
        putchar('0');
    puthex(n);
}

char getc()
{
    char c;

    uart_get_str(&c, 1);

    return c;
}

char getchar()
{
    char c = getc();

    c = (c == '\r') ? '\n' : c;
    putchar(c);

    return c;
}

char* gets(char *s)
{
    char *p = s;
    char c;

    while ((c = getchar()) != '\n')
        *p++ = c;
    *p = '\0';

    return s;
}

void printf(char *fmt, ...)
{
    char **argp = &fmt + 1;
    char *p;

    for (p = fmt; *p; p++) {
        if (*p == '%') {
            switch (*++p) {
            case 'c': {
                putchar((int)*argp);
                break;
            }
            case 's': {
                char *s = *argp;
                while (*s)
                    putchar(*s++);
                break;
            }
            case 'd': {
                putdec((int)*argp);
                break;
            }
            case 'x':
            case 'X': {
                puthex((int)*argp);
                break;
            }
            default: {
                putchar(*p);
                break;
            }
            }
            argp++;
        }
        else if (*p == '\\') {
            switch (*++p) {
            case 'n':
                putchar('\n');
                break;
            case 't':
                putchar('\t');
                break;
            }
        }
        else
            putchar(*p);
    }
}
