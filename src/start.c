#include "uros.h"
#include "system.h"

extern int main();

void start_os(void)
{
    /* System dependent initialization */
    initialize_system();

    main();
}
