#include "mwdt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t now_ms;
    bool reset_requested;
} consumer_ctx_t;

static uint32_t consumer_clock(void *ctx)
{
    consumer_ctx_t *consumer = (consumer_ctx_t *)ctx;
    return consumer->now_ms;
}

static void consumer_reset(const mwdt_timeout_t *event, void *ctx)
{
    consumer_ctx_t *consumer = (consumer_ctx_t *)ctx;
    (void)event;
    consumer->reset_requested = true;
}

int main(void)
{
    consumer_ctx_t consumer = {0};
    mwdt_t watchdog = {0};
    mwdt_task_t tasks[2] = {{0}};
    mwdt_config_t config = {0};
    size_t task_index = 0U;
    size_t timed_out = 0U;

    config.tasks = tasks;
    config.task_capacity = 2U;
    config.clock_fn = consumer_clock;
    config.clock_ctx = &consumer;
    config.reset_fn = consumer_reset;
    config.reset_ctx = &consumer;

    if (mwdt_init(&watchdog, &config) != MWDT_OK) {
        return 1;
    }
    if (mwdt_register(&watchdog, "main", 50U, 1U, false, &task_index) != MWDT_OK) {
        return 2;
    }
    if (mwdt_kick(&watchdog, task_index) != MWDT_OK) {
        return 3;
    }
    if (mwdt_check(&watchdog, &timed_out) != MWDT_OK) {
        return 4;
    }
    return timed_out == 0U ? 0 : 5;
}
