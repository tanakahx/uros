#ifndef UROS_H
#define UROS_H

#define NULL (void *)0
#define HEAP_SIZE      4096

typedef unsigned long  size_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

typedef int (*sys_call_t)(void);
typedef void (*thread_t)(int);

void start_os(void);

#endif
