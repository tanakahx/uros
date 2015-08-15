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

/* Wait queue */
typedef struct wque {
    struct wque *next;
    struct wque *prev;
} wque_t;

/* Resource Type */
typedef struct {
    uint32_t owner;
    int pri;
    wque_t wque;
} res_t;

extern res_t res[];

int debug(const char *s);
int declare_task(thread_t entry, int pri, size_t stack_size);
int activate_task(int task_id);
int terminate_task(void);
int get_resource(uint32_t res_id);
int release_resource(uint32_t res_id);
int set_event(int task_id, uint32_t event);
int clear_event(int task_id, uint32_t event);
int get_event(int task_id, uint32_t *event);
int wait_event(uint32_t event);
void start_os(void);

void uros_main(void);

#endif
