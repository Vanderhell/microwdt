#include "mwdt.h"

int mtu_b(void)
{
    mwdt_task_t task = {0};
    return task.enabled ? 1 : 0;
}
