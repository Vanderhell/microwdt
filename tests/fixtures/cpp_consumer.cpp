#include "mwdt.h"

#include <cstddef>
#include <cstdint>

static std::uint32_t cpp_clock(void *ctx)
{
    return *static_cast<std::uint32_t *>(ctx);
}

int main()
{
    std::uint32_t now = 42U;
    mwdt_t watchdog = {};
    mwdt_task_t tasks[2] = {};
    mwdt_config_t config = {};
    std::size_t index = 0U;

    config.tasks = tasks;
    config.task_capacity = 2U;
    config.clock_fn = cpp_clock;
    config.clock_ctx = &now;

    if (mwdt_init(&watchdog, &config) != MWDT_OK) {
        return 1;
    }
    if (mwdt_register(&watchdog, "cpp", 10U, 1U, false, &index) != MWDT_OK) {
        return 2;
    }
    return index == 0U ? 0 : 3;
}
