# Cookbook

This document collects small integration patterns for `microwdt`.

## Single-owner polling loop

Use one application-owned loop or task to own one watchdog instance.

```c
#include "mwdt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t now_ms;
    bool reset_requested;
} app_ctx_t;

static uint32_t app_clock(void *ctx)
{
    app_ctx_t *app = (app_ctx_t *)ctx;
    return app->now_ms;
}

static void app_reset(const mwdt_timeout_t *event, void *ctx)
{
    app_ctx_t *app = (app_ctx_t *)ctx;

    if (event->state == MWDT_TASK_STARVED) {
        app->reset_requested = true;
    }
}

int app_init(app_ctx_t *app, mwdt_t *wdt, mwdt_task_t *tasks, size_t *sensor_index)
{
    mwdt_config_t config = {0};

    config.tasks = tasks;
    config.task_capacity = 1U;
    config.clock_fn = app_clock;
    config.clock_ctx = app;
    config.reset_fn = app_reset;
    config.reset_ctx = app;

    if (mwdt_init(wdt, &config) != MWDT_OK) {
        return -1;
    }
    if (mwdt_register(wdt, "sensor", 100U, 3U, true, sensor_index) != MWDT_OK) {
        return -2;
    }
    return 0;
}
```

## Periodic check ownership

Call `mwdt_check` from one serialized owner context only.

```c
int app_poll(app_ctx_t *app, mwdt_t *wdt)
{
    size_t timed_out = 0U;

    if (mwdt_check(wdt, &timed_out) != MWDT_OK) {
        return -1;
    }
    if (app->reset_requested) {
        /* Trigger platform-specific recovery here. */
        return -2;
    }
    return timed_out == 0U ? 0 : 1;
}
```

## Worker completion kick

Retain indexes returned by `mwdt_register` and kick from serialized code paths.

```c
int sensor_completed(mwdt_t *wdt, size_t sensor_index)
{
    return mwdt_kick(wdt, sensor_index) == MWDT_OK ? 0 : -1;
}
```

If multiple execution contexts can touch one watchdog instance, add external
serialization or route mutations through one owner task.

## Reset latch handling

A returning reset callback means a reset was requested. The latch remains set
until the application explicitly clears it after recovery conditions hold.

```c
int app_clear_reset_if_safe(mwdt_t *wdt)
{
    bool requested = false;

    if (mwdt_reset_is_requested(wdt, &requested) != MWDT_OK) {
        return -1;
    }
    if (!requested) {
        return 0;
    }
    return mwdt_clear_reset_request(wdt) == MWDT_OK ? 1 : -2;
}
```

## Disabled maintenance window

Disable a task when missed deadlines are expected for a bounded maintenance
window. Re-enabling resets its timing state from the current clock sample.

```c
int app_pause_task(mwdt_t *wdt, size_t index)
{
    return mwdt_enable(wdt, index, false) == MWDT_OK ? 0 : -1;
}

int app_resume_task(mwdt_t *wdt, size_t index)
{
    return mwdt_enable(wdt, index, true) == MWDT_OK ? 0 : -1;
}
```

## Multiple independent watchdogs

Different `mwdt_t` instances can coexist if they do not share mutable storage,
callbacks, or context objects unsafely.

Typical split:

- one watchdog for fast control-loop tasks
- one watchdog for slower communication tasks
- one owner loop per watchdog instance

## What not to do

- Do not mutate `mwdt_t` or `mwdt_task_t` fields directly after initialization.
- Do not assume every timeout passes through `LATE` before `STARVED`.
- Do not call `mwdt_check` from arbitrary ISRs unless the whole access model is
  independently proven ISR-safe.
- Do not treat the software watchdog as a hardware-watchdog replacement for
  whole-system lockup detection.
