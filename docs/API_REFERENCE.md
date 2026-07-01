# API Reference

Header:

```c
#include "mwdt.h"
```

## Ownership and initialization

- `mwdt_t` is caller-allocated storage for one watchdog instance.
- `mwdt_task_t` storage is caller-owned and provided through `mwdt_config_t.tasks`.
- Public field presence is stable across translation units, but direct mutation of `mwdt_t` and `mwdt_task_t` fields outside the documented API is unsupported.
- All library state is ordinary process RAM in caller-owned objects. Reboot, power loss, faults, or process exit discard it.
- `mwdt_init` copies callback function pointers and callback context pointers by value into the watchdog instance.
- The task storage array must outlive every use of the watchdog instance until the next successful `mwdt_init`.
- Task names are borrowed `const char *` pointers. They must remain valid and immutable until reinitialization.
- Callback context pointers remain caller-owned and must outlive any callback use.
- A zeroed `mwdt_t` is not initialized. Every public API except `mwdt_init`, `mwdt_err_str`, and `mwdt_task_state_str` rejects it with `MWDT_ERR_UNINITIALIZED`.

## Clock model

- The clock callback has signature `uint32_t (*)(void *ctx)`.
- Time is interpreted as a monotonic millisecond counter with modulo-32-bit wraparound semantics.
- Deadline detection uses unsigned subtraction:
  `elapsed_ms = now_ms - last_kick_ms`.
- `elapsed_ms < deadline_ms` means on time.
- `elapsed_ms >= deadline_ms` means one or more deadlines were missed.
- Backward jumps outside wraparound semantics are unsupported.

## Registration

```c
mwdt_err_t mwdt_register(
    mwdt_t *wdt,
    const char *name,
    uint32_t deadline_ms,
    uint32_t max_misses,
    bool auto_reset,
    size_t *out_index);
```

- Rejects null watchdog, null output pointer, null name, empty name, zero deadline, duplicate name, and full task storage.
- `max_misses == 0` means warn-only. The task can become `LATE` but never `STARVED`.
- `auto_reset == true` requires `max_misses > 0` and a configured reset callback.
- Registration samples the clock once and stores the initial kick timestamp.
- Failure does not mutate task storage, task count, counters, or reset state.

## Task states and misses

- `MWDT_TASK_OK`: within deadline.
- `MWDT_TASK_LATE`: at least one deadline period elapsed and the starvation threshold was not reached.
- `MWDT_TASK_STARVED`: starvation threshold reached.
- `MWDT_TASK_DISABLED`: task monitoring disabled.

Miss-count calculation during `mwdt_check`:

```c
elapsed_ms = now_ms - last_kick_ms;
elapsed_periods = elapsed_ms / deadline_ms;
miss_count = max(old_miss_count, elapsed_periods);
```

- Miss counts do not decrease because time passes.
- Miss counts only decrease through successful kick, disable then enable, or reinitialization.
- A delayed check can transition directly from `OK` to `STARVED`.
- `STARVED` remains `STARVED` until explicit recovery.

## Checks and callbacks

```c
mwdt_err_t mwdt_check(mwdt_t *wdt, size_t *out_timed_out);
```

- Samples the clock once per call.
- `out_timed_out` receives the number of currently enabled tasks whose elapsed time is at least their deadline.
- Detection latency is bounded by the application's `mwdt_check` cadence, not just by each task deadline.
- For each transition to `LATE` or `STARVED`, the library:
  1. Computes candidate state and miss count.
  2. Commits the task state and miss count.
  3. Increments `transition_event_count`.
  4. Builds an immutable `mwdt_timeout_t` snapshot.
  5. Invokes the timeout callback if configured.
- The event pointer is valid only for the duration of the synchronous callback.
- Event fields are copied values except `name`, which is the borrowed task-name pointer.
- Query APIs called during the callback observe the already-committed state and counters.
- The library performs no further mutation of that task after the timeout callback returns.
- Return values from timeout and reset callbacks are not modeled by the API; callback-side failures do not propagate through library calls.

## Reentrancy and busy state

- During `mwdt_check`, timeout callbacks, and reset callbacks, same-instance mutating APIs return `MWDT_ERR_BUSY`.
- The blocked same-instance mutating APIs include:
  - `mwdt_init`
  - `mwdt_set_timeout_cb`
  - `mwdt_set_reset_cb`
  - `mwdt_register`
  - `mwdt_enable`
  - `mwdt_kick`
  - `mwdt_kick_by_name`
  - `mwdt_check`
  - `mwdt_clear_reset_request`
- Query-only APIs are allowed during callbacks.
- Callbacks may operate on a different watchdog instance if storage, callbacks, and contexts are independent.

## Reset request latch

- On the first eligible `STARVED` transition for an `auto_reset` task, the library:
  - sets `reset_requested`
  - records the triggering task index
  - increments `reset_request_count`
  - invokes the reset callback once if configured
  - stops scanning further tasks in that check
- If the reset callback returns, the latch remains set.
- While the latch is set:
  - `mwdt_check` returns `MWDT_ERR_RESET_LATCHED`
  - `mwdt_kick` and `mwdt_kick_by_name` return `MWDT_ERR_RESET_LATCHED`
  - no additional reset callbacks are dispatched
- `mwdt_clear_reset_request` succeeds only when every auto-reset task is `OK` or `DISABLED` and no check or callback is active.

## Enable, disable, and kick

- `mwdt_enable(..., false)` moves the task to `MWDT_TASK_DISABLED`, does not sample the clock, and emits no callback.
- Re-enabling a disabled task samples the clock once, clears misses, clears that task's reset-issued marker, and sets the task to `MWDT_TASK_OK`.
- `mwdt_kick` on an enabled task samples the clock once, clears misses, clears the task reset-issued marker, and sets the task to `MWDT_TASK_OK`.
- `mwdt_kick` on a disabled task returns `MWDT_ERR_DISABLED`.

## Queries

- `mwdt_get_all_ok` reports true only when every enabled task is `MWDT_TASK_OK`.
- `mwdt_get_task_state`, `mwdt_get_remaining`, and `mwdt_get_task` return `MWDT_ERR_NOT_FOUND` for invalid indexes.
- `mwdt_get_remaining` returns `MWDT_ERR_DISABLED` for disabled tasks and otherwise reports zero when the deadline has already expired.
- `mwdt_get_task` copies task state into `mwdt_task_snapshot_t`; it never returns a mutable pointer into internal storage.
- `mwdt_reset_is_requested` and `mwdt_get_reset_trigger_task` expose reset-latch state.

## Counters

- `check_count`: successful `mwdt_check` calls.
- `transition_event_count`: number of `LATE` or `STARVED` transition events, whether or not a timeout callback is configured.
- `reset_request_count`: number of reset requests issued.
- All three counters use saturating increment semantics at `UINT32_MAX`.

## Safety boundary

- The library is not thread-safe for concurrent access to one `mwdt_t`.
- A C data race on one watchdog instance is undefined behavior.
- The preferred model is one owner task or main loop that performs checks and serialized mutations.
- The library is not ISR-safe by default.
- Calling `mwdt_check` from a timer ISR is unsupported unless the platform independently proves the callback, clock, and access model are ISR-safe.
- Kicking tasks from arbitrary task contexts without synchronization is unsupported on a shared watchdog instance.
- The library does not own cleanup. `return`, `goto`, `break`, `continue`, `abort`, `_Exit`, faults, brownout, watchdog reset, or power loss trigger no automatic library cleanup.
- Callback errors do not propagate through the API.
- `longjmp` and C++ exceptions escaping through callbacks are unsupported.
