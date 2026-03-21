/*
 * microwdt — Implementation.
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microwdt
 */

#include "mwdt.h"
#include <string.h>

/* ── Strings ───────────────────────────────────────────────────────────── */

const char *mwdt_err_str(mwdt_err_t err)
{
    switch (err) {
    case MWDT_OK:            return "ok";
    case MWDT_ERR_NULL:      return "null pointer";
    case MWDT_ERR_FULL:      return "task table full";
    case MWDT_ERR_NOT_FOUND: return "task not found";
    case MWDT_ERR_INVALID:   return "invalid config";
    default:                 return "unknown error";
    }
}

const char *mwdt_task_state_str(mwdt_task_state_t state)
{
    switch (state) {
    case MWDT_TASK_OK:       return "OK";
    case MWDT_TASK_LATE:     return "LATE";
    case MWDT_TASK_STARVED:  return "STARVED";
    case MWDT_TASK_DISABLED: return "DISABLED";
    default:                 return "?";
    }
}

/* ── Helpers ───────────────────────────────────────────────────────────── */

static inline uint32_t elapsed(uint32_t from, uint32_t now)
{
    return now - from;  /* unsigned wrap-safe */
}

/* ── Init ──────────────────────────────────────────────────────────────── */

mwdt_err_t mwdt_init(mwdt_t *wdt, mwdt_clock_fn clock)
{
    if (wdt == NULL || clock == NULL) return MWDT_ERR_NULL;

    memset(wdt, 0, sizeof(*wdt));
    wdt->clock = clock;

    return MWDT_OK;
}

void mwdt_set_timeout_cb(mwdt_t *wdt, mwdt_timeout_fn fn, void *ctx)
{
    if (wdt == NULL) return;
    wdt->timeout_fn  = fn;
    wdt->timeout_ctx = ctx;
}

void mwdt_set_reset_cb(mwdt_t *wdt, mwdt_reset_fn fn, void *ctx)
{
    if (wdt == NULL) return;
    wdt->reset_fn  = fn;
    wdt->reset_ctx = ctx;
}

/* ── Registration ──────────────────────────────────────────────────────── */

int mwdt_register(mwdt_t *wdt, const char *name, uint32_t deadline_ms,
                   uint8_t max_misses, bool auto_reset)
{
    if (wdt == NULL || name == NULL) return MWDT_ERR_NULL;
    if (wdt->num_tasks >= MWDT_MAX_TASKS) return MWDT_ERR_FULL;
    if (deadline_ms == 0) return MWDT_ERR_INVALID;

    uint8_t idx = wdt->num_tasks;
    mwdt_task_t *t = &wdt->tasks[idx];

    t->name         = name;
    t->deadline_ms  = deadline_ms;
    t->max_misses   = max_misses;
    t->auto_reset   = auto_reset;
    t->enabled      = true;
    t->last_kick_ms = wdt->clock();
    t->miss_count   = 0;
    t->state        = MWDT_TASK_OK;

    wdt->num_tasks++;
    return (int)idx;
}

mwdt_err_t mwdt_enable(mwdt_t *wdt, uint8_t index, bool enabled)
{
    if (wdt == NULL) return MWDT_ERR_NULL;
    if (index >= wdt->num_tasks) return MWDT_ERR_NOT_FOUND;

    mwdt_task_t *t = &wdt->tasks[index];

    if (enabled && !t->enabled) {
        /* Re-enabling: reset kick time so it doesn't immediately trigger */
        t->last_kick_ms = wdt->clock();
        t->miss_count   = 0;
        t->state        = MWDT_TASK_OK;
    }

    t->enabled = enabled;
    t->state   = enabled ? t->state : MWDT_TASK_DISABLED;

    return MWDT_OK;
}

/* ── Kick ──────────────────────────────────────────────────────────────── */

mwdt_err_t mwdt_kick(mwdt_t *wdt, uint8_t index)
{
    if (wdt == NULL) return MWDT_ERR_NULL;
    if (index >= wdt->num_tasks) return MWDT_ERR_NOT_FOUND;

    mwdt_task_t *t = &wdt->tasks[index];
    if (!t->enabled) return MWDT_OK;

    t->last_kick_ms = wdt->clock();
    t->miss_count   = 0;
    t->state        = MWDT_TASK_OK;

    return MWDT_OK;
}

mwdt_err_t mwdt_kick_by_name(mwdt_t *wdt, const char *name)
{
    if (wdt == NULL || name == NULL) return MWDT_ERR_NULL;

    int idx = mwdt_find(wdt, name);
    if (idx < 0) return MWDT_ERR_NOT_FOUND;

    return mwdt_kick(wdt, (uint8_t)idx);
}

/* ── Check ─────────────────────────────────────────────────────────────── */

int mwdt_check(mwdt_t *wdt)
{
    if (wdt == NULL || wdt->clock == NULL) return 0;

    uint32_t now = wdt->clock();
    int timed_out = 0;

    for (uint8_t i = 0; i < wdt->num_tasks; i++) {
        mwdt_task_t *t = &wdt->tasks[i];

        if (!t->enabled) continue;

        uint32_t el = elapsed(t->last_kick_ms, now);

        if (el < t->deadline_ms) {
            /* Within deadline — if was late, recovery happens on kick */
            continue;
        }

        /* Deadline exceeded */
        timed_out++;

        /* How many deadlines have passed? */
        uint8_t new_misses = (uint8_t)(el / t->deadline_ms);
        if (new_misses == 0) new_misses = 1;

        /* Only process if miss count changed */
        if (new_misses == t->miss_count) continue;

        mwdt_task_state_t prev_state = t->state;
        t->miss_count = new_misses;

        /* Determine new state */
        mwdt_task_state_t new_state;
        if (t->max_misses > 0 && t->miss_count >= t->max_misses) {
            new_state = MWDT_TASK_STARVED;
        } else {
            new_state = MWDT_TASK_LATE;
        }

        /* Fire callback on state transitions */
        if (new_state != prev_state && wdt->timeout_fn != NULL) {
            mwdt_timeout_t event = {
                .task_idx     = i,
                .name         = t->name,
                .state        = new_state,
                .prev_state   = prev_state,
                .deadline_ms  = t->deadline_ms,
                .elapsed_ms   = el,
                .miss_count   = t->miss_count,
                .timestamp_ms = now,
            };
            wdt->timeout_fn(&event, wdt->timeout_ctx);
            wdt->timeout_count++;
        }

        t->state = new_state;

        /* Auto-reset on STARVED */
        if (new_state == MWDT_TASK_STARVED && t->auto_reset &&
            wdt->reset_fn != NULL) {
            wdt->reset_fn(wdt->reset_ctx);
            /* Note: if reset_fn resets the MCU, we never get here */
        }
    }

    wdt->check_count++;
    return timed_out;
}

/* ── Query ─────────────────────────────────────────────────────────────── */

uint8_t mwdt_task_count(const mwdt_t *wdt)
{
    if (wdt == NULL) return 0;
    return wdt->num_tasks;
}

const mwdt_task_t *mwdt_task_at(const mwdt_t *wdt, uint8_t index)
{
    if (wdt == NULL || index >= wdt->num_tasks) return NULL;
    return &wdt->tasks[index];
}

int mwdt_find(const mwdt_t *wdt, const char *name)
{
    if (wdt == NULL || name == NULL) return -1;

    for (uint8_t i = 0; i < wdt->num_tasks; i++) {
        if (wdt->tasks[i].name != NULL &&
            strcmp(wdt->tasks[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

mwdt_task_state_t mwdt_task_state(const mwdt_t *wdt, uint8_t index)
{
    if (wdt == NULL || index >= wdt->num_tasks) return MWDT_TASK_DISABLED;
    return wdt->tasks[index].state;
}

bool mwdt_all_ok(const mwdt_t *wdt)
{
    if (wdt == NULL) return true;

    for (uint8_t i = 0; i < wdt->num_tasks; i++) {
        if (wdt->tasks[i].enabled && wdt->tasks[i].state != MWDT_TASK_OK) {
            return false;
        }
    }
    return true;
}

uint32_t mwdt_remaining(const mwdt_t *wdt, uint8_t index)
{
    if (wdt == NULL || index >= wdt->num_tasks || wdt->clock == NULL) return 0;

    const mwdt_task_t *t = &wdt->tasks[index];
    if (!t->enabled) return 0;

    uint32_t el = elapsed(t->last_kick_ms, wdt->clock());
    if (el >= t->deadline_ms) return 0;

    return t->deadline_ms - el;
}

uint32_t mwdt_timeout_count(const mwdt_t *wdt)
{
    if (wdt == NULL) return 0;
    return wdt->timeout_count;
}

uint32_t mwdt_check_count(const mwdt_t *wdt)
{
    if (wdt == NULL) return 0;
    return wdt->check_count;
}
