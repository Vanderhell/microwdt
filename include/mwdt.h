/*
 * microwdt — Software watchdog manager for embedded systems.
 *
 * Monitor task liveness: register tasks with deadlines, kick from each task,
 * detect stalls. Bridges nvlog (log timeouts) and panicdump (crash on fatal).
 *
 * C99 · Zero dependencies · Zero allocations · Callback-driven · Portable
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microwdt
 */

#ifndef MWDT_H
#define MWDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ─────────────────────────────────────────────────────── */

/** Maximum number of watched tasks. */
#ifndef MWDT_MAX_TASKS
#define MWDT_MAX_TASKS 8
#endif

/* ── Error codes ───────────────────────────────────────────────────────── */

typedef enum {
    MWDT_OK            =  0,
    MWDT_ERR_NULL      = -1,
    MWDT_ERR_FULL      = -2,
    MWDT_ERR_NOT_FOUND = -3,
    MWDT_ERR_INVALID   = -4,
} mwdt_err_t;

const char *mwdt_err_str(mwdt_err_t err);

/* ── Task state ────────────────────────────────────────────────────────── */

typedef enum {
    MWDT_TASK_OK       = 0,   /**< Task is alive, last kick within deadline. */
    MWDT_TASK_LATE     = 1,   /**< Missed deadline once (warning).           */
    MWDT_TASK_STARVED  = 2,   /**< Missed multiple deadlines (critical).     */
    MWDT_TASK_DISABLED = 3,   /**< Task monitoring suspended.                */
} mwdt_task_state_t;

const char *mwdt_task_state_str(mwdt_task_state_t state);

/* ── Platform callback ─────────────────────────────────────────────────── */

/** Clock function — returns milliseconds. Same signature as all micro* libs. */
typedef uint32_t (*mwdt_clock_fn)(void);

/* ── Timeout callback ──────────────────────────────────────────────────── */

/** Timeout event — passed to the callback when a task misses its deadline. */
typedef struct {
    uint8_t             task_idx;       /**< Task index.                    */
    const char         *name;           /**< Task name.                     */
    mwdt_task_state_t   state;          /**< New state (LATE or STARVED).   */
    mwdt_task_state_t   prev_state;     /**< Previous state.                */
    uint32_t            deadline_ms;    /**< Configured deadline.           */
    uint32_t            elapsed_ms;     /**< Time since last kick.          */
    uint8_t             miss_count;     /**< Consecutive missed deadlines.  */
    uint32_t            timestamp_ms;   /**< When detected.                 */
} mwdt_timeout_t;

/**
 * Timeout callback — called when a task misses its deadline.
 *
 * Typical implementations:
 *   - LATE:    Log warning via microlog / nvlog
 *   - STARVED: Trigger panicdump + system reset
 */
typedef void (*mwdt_timeout_fn)(const mwdt_timeout_t *event, void *ctx);

/**
 * Reset callback — called when the watchdog decides a system reset is needed.
 * If NULL, the watchdog only reports but never resets.
 *
 * Typical: NVIC_SystemReset(), esp_restart(), exit(1)
 */
typedef void (*mwdt_reset_fn)(void *ctx);

/* ── Task descriptor ───────────────────────────────────────────────────── */

typedef struct {
    const char         *name;           /**< Task name (static string).     */
    uint32_t            deadline_ms;    /**< Maximum time between kicks.    */
    uint8_t             max_misses;     /**< Misses before STARVED (0=∞).   */
    bool                auto_reset;     /**< Call reset_fn on STARVED?      */
    bool                enabled;        /**< Is monitoring active?          */

    /* Runtime state (managed by microwdt) */
    uint32_t            last_kick_ms;   /**< Timestamp of last kick.        */
    uint8_t             miss_count;     /**< Consecutive misses.            */
    mwdt_task_state_t   state;          /**< Current state.                 */
} mwdt_task_t;

/* ── Watchdog instance ─────────────────────────────────────────────────── */

typedef struct {
    mwdt_task_t         tasks[MWDT_MAX_TASKS];
    uint8_t             num_tasks;

    mwdt_clock_fn       clock;
    mwdt_timeout_fn     timeout_fn;
    void               *timeout_ctx;
    mwdt_reset_fn       reset_fn;
    void               *reset_ctx;

    uint32_t            check_count;    /**< Total checks performed.        */
    uint32_t            timeout_count;  /**< Total timeouts detected.       */
} mwdt_t;

/* ── Init ──────────────────────────────────────────────────────────────── */

/**
 * Initialise watchdog manager.
 *
 * @param wdt    Instance (caller-allocated).
 * @param clock  Clock function (required).
 * @return MWDT_OK on success.
 */
mwdt_err_t mwdt_init(mwdt_t *wdt, mwdt_clock_fn clock);

/** Set timeout callback (fires on missed deadlines). */
void mwdt_set_timeout_cb(mwdt_t *wdt, mwdt_timeout_fn fn, void *ctx);

/** Set reset callback (fires on STARVED + auto_reset). May be NULL. */
void mwdt_set_reset_cb(mwdt_t *wdt, mwdt_reset_fn fn, void *ctx);

/* ── Task registration ─────────────────────────────────────────────────── */

/**
 * Register a task to watch.
 *
 * @param wdt          Watchdog instance.
 * @param name         Task name (static/const string).
 * @param deadline_ms  Maximum ms between kicks before timeout.
 * @param max_misses   Consecutive misses before STARVED (0 = warn only, never starve).
 * @param auto_reset   If true, call reset_fn when STARVED.
 * @return Task index (0-based) or negative error.
 */
int mwdt_register(mwdt_t *wdt, const char *name, uint32_t deadline_ms,
                   uint8_t max_misses, bool auto_reset);

/** Enable or disable a task by index. Disabled tasks are skipped. */
mwdt_err_t mwdt_enable(mwdt_t *wdt, uint8_t index, bool enabled);

/* ── Kick (task reports alive) ─────────────────────────────────────────── */

/**
 * Kick the watchdog for a specific task — "I'm alive."
 *
 * Call this periodically from each monitored task. Resets the task's
 * deadline timer and clears miss count.
 *
 * @param wdt    Watchdog instance.
 * @param index  Task index returned by mwdt_register.
 * @return MWDT_OK on success.
 */
mwdt_err_t mwdt_kick(mwdt_t *wdt, uint8_t index);

/**
 * Kick by name — slower (linear search) but convenient.
 */
mwdt_err_t mwdt_kick_by_name(mwdt_t *wdt, const char *name);

/* ── Check (call from main loop / timer) ───────────────────────────────── */

/**
 * Check all tasks for deadline violations.
 *
 * Call from the main loop or a periodic timer. For each task that has
 * exceeded its deadline since the last kick:
 *   1. Increment miss_count
 *   2. Transition state (OK→LATE, LATE→STARVED)
 *   3. Fire timeout callback on state transitions
 *   4. If STARVED + auto_reset → fire reset callback
 *
 * @param wdt  Watchdog instance.
 * @return Number of tasks currently in timeout (LATE or STARVED).
 */
int mwdt_check(mwdt_t *wdt);

/* ── Query ─────────────────────────────────────────────────────────────── */

/** Get number of registered tasks. */
uint8_t mwdt_task_count(const mwdt_t *wdt);

/** Get task descriptor by index. */
const mwdt_task_t *mwdt_task_at(const mwdt_t *wdt, uint8_t index);

/** Find task index by name. Returns -1 if not found. */
int mwdt_find(const mwdt_t *wdt, const char *name);

/** Get task state by index. */
mwdt_task_state_t mwdt_task_state(const mwdt_t *wdt, uint8_t index);

/** Are all tasks OK? */
bool mwdt_all_ok(const mwdt_t *wdt);

/** Get remaining ms before deadline for a task (0 if already expired). */
uint32_t mwdt_remaining(const mwdt_t *wdt, uint8_t index);

/** Total timeout events since init. */
uint32_t mwdt_timeout_count(const mwdt_t *wdt);

/** Total checks performed. */
uint32_t mwdt_check_count(const mwdt_t *wdt);

#ifdef __cplusplus
}
#endif

#endif /* MWDT_H */
