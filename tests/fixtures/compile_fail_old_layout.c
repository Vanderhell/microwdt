#define MWDT_MAX_TASKS 4
#include "mwdt.h"

int main(void)
{
    mwdt_t watchdog = { .tasks[MWDT_MAX_TASKS - 1] = {0} };
    return watchdog.initialized ? 0 : 1;
}
