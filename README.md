# microwdt

[![CI](https://github.com/Vanderhell/microwdt/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Vanderhell/microwdt/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)

Software watchdog manager for embedded systems.

C99 | Zero dependencies | Zero allocations | Callback-driven | Portable

## Why microwdt?

A hardware watchdog detects full-system lockups, but many failures are partial. One task can stall while the rest of the system still runs.

`microwdt` monitors each task independently. If a task misses its deadline, you can identify exactly which task is stuck, how long it has been stalled, and decide what action to take (log, collect diagnostics, or reset).

## Features

- Per-task deadlines (`mwdt_register`).
- Kick-based liveness (`mwdt_kick`, `mwdt_kick_by_name`).
- State machine: `OK -> LATE -> STARVED`.
- Edge-triggered timeout callback on state transitions.
- Configurable escalation via `max_misses`.
- Optional per-task auto-reset on `STARVED`.
- Runtime enable/disable for known idle phases.
- Query helpers (`mwdt_remaining`, `mwdt_all_ok`, `mwdt_task_state`).

## Quick Start

```c
#include "mwdt.h"

static uint32_t platform_millis(void);
static void on_timeout(const mwdt_timeout_t *evt, void *ctx);
static void system_reset(void *ctx);

void app_init(void) {
    mwdt_t wdt;
    mwdt_init(&wdt, platform_millis);
    mwdt_set_timeout_cb(&wdt, on_timeout, NULL);
    mwdt_set_reset_cb(&wdt, system_reset, NULL);

    int sensor = mwdt_register(&wdt, "sensor", 2000, 3, false);
    int mqtt   = mwdt_register(&wdt, "mqtt",   5000, 2, true);

    (void)sensor;
    (void)mqtt;
}
```

## Build and Test

From repository root:

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude src/mwdt.c tests/test_all.c -o tests/test_all
./tests/test_all
```

## Project Structure

- `include/mwdt.h` public API.
- `src/mwdt.c` implementation.
- `tests/test_all.c` test suite.
- `docs/` design and reference docs.

## Documentation

- [API reference](docs/API_REFERENCE.md)
- [Design notes](docs/DESIGN.md)
- [Porting guide](docs/PORTING_GUIDE.md)
- [Contributing guide](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)

## Ecosystem

- [nvlog](https://github.com/Vanderhell/nvlog)
- [panicdump](https://github.com/Vanderhell/panicdump)
- [microlog](https://github.com/Vanderhell/microlog)
- [microhealth](https://github.com/Vanderhell/microhealth)
- [microsh](https://github.com/Vanderhell/microsh)

## License

MIT, see [LICENSE](LICENSE).

