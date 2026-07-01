#include "mwdt.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t now_ms;
    int reset_requested;
} app_ctx_t;

/* cppcheck-suppress constParameterCallback */
static uint32_t app_clock(void *ctx)
{
    const app_ctx_t *app = (const app_ctx_t *)ctx;
    return app->now_ms;
}

static void app_timeout(const mwdt_timeout_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
}

static void app_reset(const mwdt_timeout_t *event, void *ctx)
{
    app_ctx_t *app = (app_ctx_t *)ctx;
    if (event->state == MWDT_TASK_STARVED) {
        app->reset_requested = 1;
    }
}

int main(void)
{
    app_ctx_t app = {0};
    mwdt_t watchdog = {0};
    mwdt_task_t tasks[2] = {{0}};
    mwdt_config_t config = {0};
    size_t sensor_index = 0U;
    size_t mqtt_index = 0U;
    size_t timed_out = 0U;
    bool reset_latched = false;

    config.tasks = tasks;
    config.task_capacity = 2U;
    config.clock_fn = app_clock;
    config.clock_ctx = &app;
    config.timeout_fn = app_timeout;
    config.timeout_ctx = &app;
    config.reset_fn = app_reset;
    config.reset_ctx = &app;

    if (mwdt_init(&watchdog, &config) != MWDT_OK) {
        return 1;
    }
    if (mwdt_register(&watchdog, "sensor", 2000U, 3U, false, &sensor_index) != MWDT_OK) {
        return 2;
    }
    if (mwdt_register(&watchdog, "mqtt", 5000U, 2U, true, &mqtt_index) != MWDT_OK) {
        return 3;
    }
    if (mwdt_kick(&watchdog, sensor_index) != MWDT_OK) {
        return 4;
    }
    if (mwdt_kick(&watchdog, mqtt_index) != MWDT_OK) {
        return 5;
    }
    if (mwdt_check(&watchdog, &timed_out) != MWDT_OK) {
        return 6;
    }
    if (mwdt_reset_is_requested(&watchdog, &reset_latched) != MWDT_OK) {
        return 7;
    }
    if (reset_latched) {
        return 8;
    }
    return timed_out == 0U ? 0 : 9;
}
