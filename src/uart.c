#include "uart_hal.h"

void putc(char c)
{
    uart_hal_send(c);
}

void putchar(char c)
{
    if (c == '\n')
        putc('\r');
    putc(c);
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

    uart_hal_recv(&c);

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
