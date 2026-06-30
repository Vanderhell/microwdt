#include "mwdt.h"

#include <string.h>

static uint32_t mwdt_saturating_inc(uint32_t value)
{
    return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static bool mwdt_is_mutation_blocked(const mwdt_t *wdt)
{
    return wdt->busy;
}

static mwdt_err_t mwdt_require_initialized(const mwdt_t *wdt)
{
    if (wdt == NULL) {
        return MWDT_ERR_NULL;
    }
    if (!wdt->initialized) {
        return MWDT_ERR_UNINITIALIZED;
    }
    return MWDT_OK;
}

static mwdt_err_t mwdt_require_mutable(mwdt_t *wdt)
{
    mwdt_err_t err;

    err = mwdt_require_initialized(wdt);
    if (err != MWDT_OK) {
        return err;
    }
    if (mwdt_is_mutation_blocked(wdt)) {
        return MWDT_ERR_BUSY;
    }
    return MWDT_OK;
}

static mwdt_err_t mwdt_validate_index(const mwdt_t *wdt, size_t index)
{
    if (index >= wdt->task_count) {
        return MWDT_ERR_NOT_FOUND;
    }
    return MWDT_OK;
}

static uint32_t mwdt_elapsed(uint32_t from_ms, uint32_t now_ms)
{
    return now_ms - from_ms;
}

static void mwdt_reset_runtime_task(mwdt_task_t *task, uint32_t now_ms)
{
    task->last_kick_ms = now_ms;
    task->miss_count = 0U;
    task->state = MWDT_TASK_OK;
    task->enabled = true;
    task->reset_issued = false;
}

const char *mwdt_err_str(mwdt_err_t err)
{
    switch (err) {
    case MWDT_OK:
        return "ok";
    case MWDT_ERR_NULL:
        return "null pointer";
    case MWDT_ERR_INVALID:
        return "invalid argument";
    case MWDT_ERR_FULL:
        return "task storage full";
    case MWDT_ERR_NOT_FOUND:
        return "task not found";
    case MWDT_ERR_DISABLED:
        return "task disabled";
    case MWDT_ERR_UNINITIALIZED:
        return "watchdog uninitialized";
    case MWDT_ERR_BUSY:
        return "watchdog busy";
    case MWDT_ERR_RESET_LATCHED:
        return "reset request latched";
    case MWDT_ERR_STATE:
        return "invalid watchdog state";
    default:
        return "unknown error";
    }
}

const char *mwdt_task_state_str(mwdt_task_state_t state)
{
    switch (state) {
    case MWDT_TASK_OK:
        return "OK";
    case MWDT_TASK_LATE:
        return "LATE";
    case MWDT_TASK_STARVED:
        return "STARVED";
    case MWDT_TASK_DISABLED:
        return "DISABLED";
    default:
        return "?";
    }
}

mwdt_err_t mwdt_init(mwdt_t *wdt, const mwdt_config_t *config)
{
    if (wdt == NULL || config == NULL) {
        return MWDT_ERR_NULL;
    }
    if (wdt->initialized && wdt->busy) {
        return MWDT_ERR_BUSY;
    }
    if (config->clock_fn == NULL) {
        return MWDT_ERR_INVALID;
    }
    if (config->task_capacity == 0U) {
        return MWDT_ERR_INVALID;
    }
    if (config->tasks == NULL) {
        return MWDT_ERR_INVALID;
    }

    memset(wdt, 0, sizeof(*wdt));
    memset(config->tasks, 0, config->task_capacity * sizeof(config->tasks[0]));

    wdt->tasks = config->tasks;
    wdt->task_capacity = config->task_capacity;
    wdt->task_count = 0U;
    wdt->clock_fn = config->clock_fn;
    wdt->clock_ctx = config->clock_ctx;
    wdt->timeout_fn = config->timeout_fn;
    wdt->timeout_ctx = config->timeout_ctx;
    wdt->reset_fn = config->reset_fn;
    wdt->reset_ctx = config->reset_ctx;
    wdt->reset_task_index = 0U;
    wdt->initialized = true;
    return MWDT_OK;
}

mwdt_err_t mwdt_set_timeout_cb(mwdt_t *wdt, mwdt_timeout_fn fn, void *ctx)
{
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }

    wdt->timeout_fn = fn;
    wdt->timeout_ctx = ctx;
    return MWDT_OK;
}

mwdt_err_t mwdt_set_reset_cb(mwdt_t *wdt, mwdt_reset_fn fn, void *ctx)
{
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }

    wdt->reset_fn = fn;
    wdt->reset_ctx = ctx;
    return MWDT_OK;
}

mwdt_err_t mwdt_register(
    mwdt_t *wdt,
    const char *name,
    uint32_t deadline_ms,
    uint32_t max_misses,
    bool auto_reset,
    size_t *out_index)
{
    size_t index;
    uint32_t now_ms;
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (name == NULL || out_index == NULL) {
        return MWDT_ERR_NULL;
    }
    if (name[0] == '\0' || deadline_ms == 0U) {
        return MWDT_ERR_INVALID;
    }
    if (auto_reset && max_misses == 0U) {
        return MWDT_ERR_INVALID;
    }
    if (auto_reset && wdt->reset_fn == NULL) {
        return MWDT_ERR_INVALID;
    }
    if (wdt->task_count >= wdt->task_capacity) {
        return MWDT_ERR_FULL;
    }
    if (mwdt_find(wdt, name, out_index) == MWDT_OK) {
        return MWDT_ERR_INVALID;
    }

    now_ms = wdt->clock_fn(wdt->clock_ctx);
    index = wdt->task_count;

    memset(&wdt->tasks[index], 0, sizeof(wdt->tasks[index]));
    wdt->tasks[index].name = name;
    wdt->tasks[index].deadline_ms = deadline_ms;
    wdt->tasks[index].max_misses = max_misses;
    wdt->tasks[index].auto_reset = auto_reset;
    mwdt_reset_runtime_task(&wdt->tasks[index], now_ms);

    wdt->task_count++;
    *out_index = index;
    return MWDT_OK;
}

mwdt_err_t mwdt_find(const mwdt_t *wdt, const char *name, size_t *out_index)
{
    size_t index;
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (name == NULL || out_index == NULL) {
        return MWDT_ERR_NULL;
    }
    if (name[0] == '\0') {
        return MWDT_ERR_INVALID;
    }

    for (index = 0U; index < wdt->task_count; ++index) {
        if (wdt->tasks[index].name != NULL && strcmp(wdt->tasks[index].name, name) == 0) {
            *out_index = index;
            return MWDT_OK;
        }
    }
    return MWDT_ERR_NOT_FOUND;
}

mwdt_err_t mwdt_enable(mwdt_t *wdt, size_t index, bool enabled)
{
    uint32_t now_ms;
    mwdt_task_t *task;
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    err = mwdt_validate_index(wdt, index);
    if (err != MWDT_OK) {
        return err;
    }

    task = &wdt->tasks[index];
    if (!enabled) {
        task->enabled = false;
        task->state = MWDT_TASK_DISABLED;
        return MWDT_OK;
    }

    if (task->enabled) {
        return MWDT_OK;
    }

    now_ms = wdt->clock_fn(wdt->clock_ctx);
    mwdt_reset_runtime_task(task, now_ms);
    return MWDT_OK;
}

mwdt_err_t mwdt_kick(mwdt_t *wdt, size_t index)
{
    uint32_t now_ms;
    mwdt_task_t *task;
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (wdt->reset_requested) {
        return MWDT_ERR_RESET_LATCHED;
    }
    err = mwdt_validate_index(wdt, index);
    if (err != MWDT_OK) {
        return err;
    }

    task = &wdt->tasks[index];
    if (!task->enabled) {
        return MWDT_ERR_DISABLED;
    }

    now_ms = wdt->clock_fn(wdt->clock_ctx);
    mwdt_reset_runtime_task(task, now_ms);
    return MWDT_OK;
}

mwdt_err_t mwdt_kick_by_name(mwdt_t *wdt, const char *name)
{
    size_t index;
    mwdt_err_t err = mwdt_find(wdt, name, &index);

    if (err != MWDT_OK) {
        return err;
    }
    return mwdt_kick(wdt, index);
}

static bool mwdt_all_auto_reset_tasks_recovered(const mwdt_t *wdt)
{
    size_t index;

    for (index = 0U; index < wdt->task_count; ++index) {
        const mwdt_task_t *task = &wdt->tasks[index];

        if (!task->auto_reset) {
            continue;
        }
        if (task->state != MWDT_TASK_OK && task->state != MWDT_TASK_DISABLED) {
            return false;
        }
    }
    return true;
}

mwdt_err_t mwdt_check(mwdt_t *wdt, size_t *out_timed_out)
{
    uint32_t now_ms;
    size_t timed_out = 0U;
    size_t index;
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_timed_out == NULL) {
        return MWDT_ERR_NULL;
    }
    if (wdt->reset_requested) {
        return MWDT_ERR_RESET_LATCHED;
    }

    now_ms = wdt->clock_fn(wdt->clock_ctx);
    wdt->busy = true;

    for (index = 0U; index < wdt->task_count; ++index) {
        mwdt_task_t *task = &wdt->tasks[index];
        uint32_t elapsed_ms;
        uint32_t elapsed_periods;
        uint32_t miss_count;
        mwdt_task_state_t next_state;
        mwdt_task_state_t prev_state;
        bool transitioned;

        if (!task->enabled) {
            continue;
        }

        elapsed_ms = mwdt_elapsed(task->last_kick_ms, now_ms);
        if (elapsed_ms < task->deadline_ms) {
            continue;
        }

        timed_out++;
        elapsed_periods = elapsed_ms / task->deadline_ms;
        miss_count = task->miss_count;
        if (elapsed_periods > miss_count) {
            miss_count = elapsed_periods;
        }
        if (miss_count == 0U) {
            miss_count = 1U;
        }

        prev_state = task->state;
        next_state = MWDT_TASK_LATE;
        if (task->max_misses > 0U && miss_count >= task->max_misses) {
            next_state = MWDT_TASK_STARVED;
        }
        if (prev_state == MWDT_TASK_STARVED) {
            next_state = MWDT_TASK_STARVED;
        }

        task->miss_count = miss_count;
        task->state = next_state;
        transitioned = next_state != prev_state;

        if (transitioned) {
            mwdt_timeout_t event;

            wdt->transition_event_count = mwdt_saturating_inc(wdt->transition_event_count);
            event.task_index = index;
            event.name = task->name;
            event.state = next_state;
            event.prev_state = prev_state;
            event.deadline_ms = task->deadline_ms;
            event.elapsed_ms = elapsed_ms;
            event.miss_count = miss_count;
            event.timestamp_ms = now_ms;

            if (wdt->timeout_fn != NULL) {
                wdt->timeout_fn(&event, wdt->timeout_ctx);
            }

            if (next_state == MWDT_TASK_STARVED && task->auto_reset && !wdt->reset_requested) {
                task->reset_issued = true;
                wdt->reset_requested = true;
                wdt->reset_task_index = index;
                wdt->reset_request_count = mwdt_saturating_inc(wdt->reset_request_count);
                if (wdt->reset_fn != NULL) {
                    wdt->reset_fn(&event, wdt->reset_ctx);
                }
                break;
            }
        }
    }

    wdt->busy = false;
    wdt->check_count = mwdt_saturating_inc(wdt->check_count);
    *out_timed_out = timed_out;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_task_count(const mwdt_t *wdt, size_t *out_count)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_count == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_count = wdt->task_count;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_all_ok(const mwdt_t *wdt, bool *out_all_ok)
{
    size_t index;
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_all_ok == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_all_ok = true;
    for (index = 0U; index < wdt->task_count; ++index) {
        if (wdt->tasks[index].enabled && wdt->tasks[index].state != MWDT_TASK_OK) {
            *out_all_ok = false;
            break;
        }
    }
    return MWDT_OK;
}

mwdt_err_t mwdt_get_task_state(const mwdt_t *wdt, size_t index, mwdt_task_state_t *out_state)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_state == NULL) {
        return MWDT_ERR_NULL;
    }
    err = mwdt_validate_index(wdt, index);
    if (err != MWDT_OK) {
        return err;
    }

    *out_state = wdt->tasks[index].state;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_remaining(const mwdt_t *wdt, size_t index, uint32_t *out_remaining_ms)
{
    const mwdt_task_t *task;
    uint32_t elapsed_ms;
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_remaining_ms == NULL) {
        return MWDT_ERR_NULL;
    }
    err = mwdt_validate_index(wdt, index);
    if (err != MWDT_OK) {
        return err;
    }

    task = &wdt->tasks[index];
    if (!task->enabled) {
        return MWDT_ERR_DISABLED;
    }

    elapsed_ms = mwdt_elapsed(task->last_kick_ms, wdt->clock_fn(wdt->clock_ctx));
    if (elapsed_ms >= task->deadline_ms) {
        *out_remaining_ms = 0U;
    } else {
        *out_remaining_ms = task->deadline_ms - elapsed_ms;
    }
    return MWDT_OK;
}

mwdt_err_t mwdt_get_task(const mwdt_t *wdt, size_t index, mwdt_task_snapshot_t *out_task)
{
    const mwdt_task_t *task;
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_task == NULL) {
        return MWDT_ERR_NULL;
    }
    err = mwdt_validate_index(wdt, index);
    if (err != MWDT_OK) {
        return err;
    }

    task = &wdt->tasks[index];
    out_task->name = task->name;
    out_task->deadline_ms = task->deadline_ms;
    out_task->max_misses = task->max_misses;
    out_task->last_kick_ms = task->last_kick_ms;
    out_task->miss_count = task->miss_count;
    out_task->auto_reset = task->auto_reset;
    out_task->enabled = task->enabled;
    out_task->reset_issued = task->reset_issued;
    out_task->state = task->state;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_check_count(const mwdt_t *wdt, uint32_t *out_count)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_count == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_count = wdt->check_count;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_transition_event_count(const mwdt_t *wdt, uint32_t *out_count)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_count == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_count = wdt->transition_event_count;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_reset_request_count(const mwdt_t *wdt, uint32_t *out_count)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_count == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_count = wdt->reset_request_count;
    return MWDT_OK;
}

mwdt_err_t mwdt_reset_is_requested(const mwdt_t *wdt, bool *out_requested)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_requested == NULL) {
        return MWDT_ERR_NULL;
    }

    *out_requested = wdt->reset_requested;
    return MWDT_OK;
}

mwdt_err_t mwdt_get_reset_trigger_task(const mwdt_t *wdt, size_t *out_index)
{
    mwdt_err_t err = mwdt_require_initialized(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (out_index == NULL) {
        return MWDT_ERR_NULL;
    }
    if (!wdt->reset_requested) {
        return MWDT_ERR_STATE;
    }

    *out_index = wdt->reset_task_index;
    return MWDT_OK;
}

mwdt_err_t mwdt_clear_reset_request(mwdt_t *wdt)
{
    mwdt_err_t err = mwdt_require_mutable(wdt);

    if (err != MWDT_OK) {
        return err;
    }
    if (!wdt->reset_requested) {
        return MWDT_OK;
    }
    if (!mwdt_all_auto_reset_tasks_recovered(wdt)) {
        return MWDT_ERR_STATE;
    }

    wdt->reset_requested = false;
    wdt->reset_task_index = 0U;
    return MWDT_OK;
}
