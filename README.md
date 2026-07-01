# microwdt

[![CI](https://github.com/Vanderhell/microwdt/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Vanderhell/microwdt/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)

`microwdt` is a small software watchdog manager for embedded-style applications.
It tracks caller-owned tasks, samples a caller-owned monotonic 32-bit millisecond clock, and reports task deadline misses through synchronous callbacks.

## Scope

- C99 minimum, C11-compatible.
- C++ consumers supported through the public header and a C-built library.
- No heap allocation.
- No OS, RTOS, filesystem, scheduler, logging, or hardware watchdog dependency.
- No thread-safety or ISR-safety guarantee for one `mwdt_t`.
- All watchdog and task state lives in volatile process RAM owned by the application.
- Public structure fields have stable presence across translation units, but direct mutation of `mwdt_t` and `mwdt_task_t` fields outside the API is unsupported.

## State model

- `MWDT_TASK_OK`: last kick was within the deadline.
- `MWDT_TASK_LATE`: at least one deadline period elapsed without a kick.
- `MWDT_TASK_STARVED`: `max_misses` threshold was reached.
- `MWDT_TASK_DISABLED`: monitoring is suspended for that task.

A delayed check can move a task directly from `OK` to `STARVED`. `STARVED` remains `STARVED` until the task is kicked, disabled then re-enabled, or the watchdog is reinitialized.

## Quick start

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

static void app_timeout(const mwdt_timeout_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
}

static void app_reset(const mwdt_timeout_t *event, void *ctx)
{
    app_ctx_t *app = (app_ctx_t *)ctx;
    if (event->state == MWDT_TASK_STARVED) {
        app->reset_requested = true;
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
        /* Request platform-specific recovery from the serialized owner context. */
        return 8;
    }

    return timed_out == 0U ? 0 : 9;
}
```

## Build and test

Manual compile:

```sh
clang-cl /nologo /std:c11 /W4 /WX /Iinclude src\mwdt.c tests\test_all.c /Fe:tests\test_all.exe
tests\test_all.exe
```

CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Runtime model and limits

- One application-owned control loop should own each `mwdt_t`.
- Detection latency depends on how often that owner calls `mwdt_check`.
- Access to one watchdog from multiple threads, tasks, or ISRs requires external serialization or message passing.
- A C data race on the same watchdog object is undefined behavior.
- `mwdt_check` samples the clock once per check. `mwdt_kick`, re-enable, and registration sample it once per operation.
- The clock must be monotonic modulo 32 bits. Backward jumps outside wraparound semantics are unsupported.
- Timeout and reset callbacks are synchronous. Errors from those callbacks do not propagate through the API.
- Timeout and reset callback contexts are borrowed caller-owned pointers.
- A returned reset callback means a reset was requested, not that the platform actually reset.
- The library cannot detect failure of the code that is responsible for calling `mwdt_check`.
- Full scheduler lockup, disabled interrupts preventing checks, and CPU lockup are outside the reach of a software watchdog. Use an independent hardware watchdog for whole-system liveness.

## Repository layout

- `include/mwdt.h`: public API
- `src/mwdt.c`: implementation
- `tests/`: runtime tests, compile fixtures, and external consumer fixtures
- `docs/`: API and design notes

## Documentation

- [API reference](docs/API_REFERENCE.md)
- [Design notes](docs/DESIGN.md)
- [Porting guide](docs/PORTING_GUIDE.md)
- [Contributing guide](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)

## License

MIT, see [LICENSE](LICENSE).
