#include "mwdt.h"

static uint32_t wrong_clock(void)
{
    return 0U;
}

int main(void)
{
    mwdt_t watchdog = {0};
    mwdt_task_t tasks[1] = {{0}};
    mwdt_config_t config = {0};

    config.tasks = tasks;
    config.task_capacity = 1U;
    config.clock_fn = wrong_clock;
    return mwdt_init(&watchdog, &config);
}
