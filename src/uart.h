#ifndef __UART_H__
#define __UART_H__

#include "system.h"

void putchar(char c);
void puts(const char *s);
void putdec(unsigned int n);
void puthex(unsigned int n);
void puthex_n(unsigned int n, int column);

char getc();
char getchar();
char* gets(char *s);

void printf(char *fmt, ...);

#endif
