# Design Notes

## Caller-owned storage

`microwdt` keeps the public watchdog size stable by requiring the caller to provide task storage. That removes any public ABI dependence on configuration macros and lets different translation units agree on the same `mwdt_t` layout.
The stable public field set exists for ABI consistency, not to invite direct field writes. Applications should treat `mwdt_t` and `mwdt_task_t` as API-owned state once initialized or registered.

## State transitions

The library tracks four states:

- `OK`
- `LATE`
- `STARVED`
- `DISABLED`

Checks do not force a gradual `OK -> LATE -> STARVED` sequence. If enough deadline periods elapsed before the next check, a task can go directly to `STARVED`.

## Miss-count semantics

The implementation uses modulo-32-bit unsigned time arithmetic. A check computes elapsed periods from the last successful kick and never narrows the result to 8-bit storage. Miss counts remain monotonic until explicit recovery.

## Callback ordering

Timeout callbacks run after the new state and miss count are committed. Query APIs inside the callback therefore observe the same state that the callback event reports.

Reset callbacks are request notifications, not proofs of a completed reset. If the callback returns, the watchdog remains latched until the application reinitializes it or explicitly clears the reset request after all auto-reset tasks are recovered or disabled.

## Concurrency boundary

`microwdt` does not add mutexes, atomics, RTOS wrappers, or ISR abstractions. One watchdog instance expects one serialized owner. Different instances are independent only if their storage, callbacks, and context objects are also independent.

## Software-watchdog limits

This library can detect missed deadlines only when the code that owns the watchdog continues to run checks. It cannot detect:

- failure of the code responsible for calling `mwdt_check`
- full scheduler lockup
- interrupts disabled long enough to prevent checks
- CPU lockup

For whole-system liveness, pair it with an independent hardware watchdog outside this library.
Detection latency also depends on the owner's check cadence, so a slow polling loop can delay both `LATE` and `STARVED` observation.
