#ifndef MICROWDT_MWDT_H_INCLUDED
#define MICROWDT_MWDT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MWDT_OK = 0,
    MWDT_ERR_NULL = -1,
    MWDT_ERR_INVALID = -2,
    MWDT_ERR_FULL = -3,
    MWDT_ERR_NOT_FOUND = -4,
    MWDT_ERR_DISABLED = -5,
    MWDT_ERR_UNINITIALIZED = -6,
    MWDT_ERR_BUSY = -7,
    MWDT_ERR_RESET_LATCHED = -8,
    MWDT_ERR_STATE = -9
} mwdt_err_t;

typedef enum {
    MWDT_TASK_OK = 0,
    MWDT_TASK_LATE = 1,
    MWDT_TASK_STARVED = 2,
    MWDT_TASK_DISABLED = 3
} mwdt_task_state_t;

typedef uint32_t (*mwdt_clock_fn)(void *ctx);

typedef struct mwdt_timeout {
    size_t task_index;
    const char *name;
    mwdt_task_state_t state;
    mwdt_task_state_t prev_state;
    uint32_t deadline_ms;
    uint32_t elapsed_ms;
    uint32_t miss_count;
    uint32_t timestamp_ms;
} mwdt_timeout_t;

typedef void (*mwdt_timeout_fn)(const mwdt_timeout_t *event, void *ctx);
typedef void (*mwdt_reset_fn)(const mwdt_timeout_t *event, void *ctx);

typedef struct {
    const char *name;
    uint32_t deadline_ms;
    uint32_t max_misses;
    uint32_t last_kick_ms;
    uint32_t miss_count;
    bool auto_reset;
    bool enabled;
    bool reset_issued;
    mwdt_task_state_t state;
} mwdt_task_t;

typedef struct {
    mwdt_task_t *tasks;
    size_t task_capacity;
    mwdt_clock_fn clock_fn;
    void *clock_ctx;
    mwdt_timeout_fn timeout_fn;
    void *timeout_ctx;
    mwdt_reset_fn reset_fn;
    void *reset_ctx;
} mwdt_config_t;

typedef struct {
    const char *name;
    uint32_t deadline_ms;
    uint32_t max_misses;
    uint32_t last_kick_ms;
    uint32_t miss_count;
    bool auto_reset;
    bool enabled;
    bool reset_issued;
    mwdt_task_state_t state;
} mwdt_task_snapshot_t;

typedef struct {
    mwdt_task_t *tasks;
    size_t task_capacity;
    size_t task_count;
    mwdt_clock_fn clock_fn;
    void *clock_ctx;
    mwdt_timeout_fn timeout_fn;
    void *timeout_ctx;
    mwdt_reset_fn reset_fn;
    void *reset_ctx;
    uint32_t check_count;
    uint32_t transition_event_count;
    uint32_t reset_request_count;
    size_t reset_task_index;
    bool initialized;
    bool busy;
    bool reset_requested;
} mwdt_t;

const char *mwdt_err_str(mwdt_err_t err);
const char *mwdt_task_state_str(mwdt_task_state_t state);

mwdt_err_t mwdt_init(mwdt_t *wdt, const mwdt_config_t *config);
mwdt_err_t mwdt_set_timeout_cb(mwdt_t *wdt, mwdt_timeout_fn fn, void *ctx);
mwdt_err_t mwdt_set_reset_cb(mwdt_t *wdt, mwdt_reset_fn fn, void *ctx);

mwdt_err_t mwdt_register(
    mwdt_t *wdt,
    const char *name,
    uint32_t deadline_ms,
    uint32_t max_misses,
    bool auto_reset,
    size_t *out_index);

mwdt_err_t mwdt_find(const mwdt_t *wdt, const char *name, size_t *out_index);
mwdt_err_t mwdt_enable(mwdt_t *wdt, size_t index, bool enabled);
mwdt_err_t mwdt_kick(mwdt_t *wdt, size_t index);
mwdt_err_t mwdt_kick_by_name(mwdt_t *wdt, const char *name);
mwdt_err_t mwdt_check(mwdt_t *wdt, size_t *out_timed_out);

mwdt_err_t mwdt_get_task_count(const mwdt_t *wdt, size_t *out_count);
mwdt_err_t mwdt_get_all_ok(const mwdt_t *wdt, bool *out_all_ok);
mwdt_err_t mwdt_get_task_state(const mwdt_t *wdt, size_t index, mwdt_task_state_t *out_state);
mwdt_err_t mwdt_get_remaining(const mwdt_t *wdt, size_t index, uint32_t *out_remaining_ms);
mwdt_err_t mwdt_get_task(const mwdt_t *wdt, size_t index, mwdt_task_snapshot_t *out_task);
mwdt_err_t mwdt_get_check_count(const mwdt_t *wdt, uint32_t *out_count);
mwdt_err_t mwdt_get_transition_event_count(const mwdt_t *wdt, uint32_t *out_count);
mwdt_err_t mwdt_get_reset_request_count(const mwdt_t *wdt, uint32_t *out_count);
mwdt_err_t mwdt_reset_is_requested(const mwdt_t *wdt, bool *out_requested);
mwdt_err_t mwdt_get_reset_trigger_task(const mwdt_t *wdt, size_t *out_index);
mwdt_err_t mwdt_clear_reset_request(mwdt_t *wdt);

#ifdef __cplusplus
}
#endif

#endif
