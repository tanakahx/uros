#ifndef LIB_H
#define LIB_H

#include "stdtype.h"

void *memset(void *b, int c, size_t len);
void *memcpy(void *dst, const void *src, size_t n);

void *mem_alloc(size_t size);
void mem_free(void *addr);

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
