# Porting Guide

## Minimum integration steps

1. Provide storage for one `mwdt_t` and a caller-owned `mwdt_task_t` array.
2. Provide a monotonic 32-bit millisecond clock callback with signature:

```c
uint32_t clock_fn(void *ctx);
```

3. Initialize the watchdog with `mwdt_init`.
4. Register tasks and retain their returned indexes.
5. Kick tasks when they complete useful work.
6. Run `mwdt_check` from one serialized owner context.

## CMake integration

```cmake
find_package(microwdt CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE microwdt::microwdt)
```

## Direct source integration

```cmake
add_library(microwdt STATIC path/to/src/mwdt.c)
target_include_directories(microwdt PUBLIC path/to/include)
target_compile_features(microwdt PUBLIC c_std_99)
```

## Platform responsibilities

- Provide a clock source with wraparound-compatible monotonic semantics.
- Serialize access if multiple tasks or threads can touch the same watchdog instance.
- Decide what the timeout callback should do with `LATE` and `STARVED` events.
- Decide what the reset callback should do when an auto-reset task starves.
- Pair the software watchdog with an independent hardware watchdog if whole-system recovery is required.

## Unsupported assumptions

- No implicit thread safety
- No implicit ISR safety
- No automatic cleanup or resource rollback
- No built-in hardware reset implementation
