/*
 * microwdt test suite.
 *
 * Build: gcc -std=c99 -Wall -Wextra -I../include ../src/mwdt.c test_all.c -o test_all
 */

#include "mwdt.h"
#include <stdio.h>
#include <string.h>

/* ── Test framework ────────────────────────────────────────────────────── */

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do {                                     \
    tests_run++;                                                \
    printf("  %-55s ", #name);                                  \
    name();                                                     \
    printf("PASS\n");                                           \
    tests_passed++;                                             \
} while (0)

#define ASSERT_EQ(expected, actual) do {                        \
    if ((expected) != (actual)) {                               \
        printf("FAIL\n    %s:%d: expected %d, got %d\n",       \
               __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_TRUE(expr) do {                                  \
    if (!(expr)) {                                              \
        printf("FAIL\n    %s:%d: expected true\n",              \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_FALSE(expr) do {                                 \
    if ((expr)) {                                               \
        printf("FAIL\n    %s:%d: expected false\n",             \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_STR_EQ(expected, actual) do {                    \
    if (strcmp((expected), (actual)) != 0) {                     \
        printf("FAIL\n    %s:%d: expected \"%s\", got \"%s\"\n",\
               __FILE__, __LINE__, (expected), (actual));       \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_GE(val, minimum) do {                            \
    if ((int)(val) < (int)(minimum)) {                          \
        printf("FAIL\n    %s:%d: %d < %d\n",                   \
               __FILE__, __LINE__, (int)(val), (int)(minimum)); \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

/* ── Mock clock ────────────────────────────────────────────────────────── */

static uint32_t mock_time = 1000;
static uint32_t mock_clock(void) { return mock_time; }

/* ── Timeout tracking ──────────────────────────────────────────────────── */

#define MAX_EVENTS 32
static mwdt_timeout_t event_log[MAX_EVENTS];
static int event_count = 0;

static void on_timeout(const mwdt_timeout_t *evt, void *ctx)
{
    (void)ctx;
    if (event_count < MAX_EVENTS) {
        event_log[event_count++] = *evt;
    }
}

/* ── Reset tracking ────────────────────────────────────────────────────── */

static int reset_count = 0;
static void on_reset(void *ctx)
{
    (void)ctx;
    reset_count++;
}

/* ── Setup ─────────────────────────────────────────────────────────────── */

static mwdt_t wdt;

static void reset_all(void)
{
    mock_time = 1000;
    event_count = 0;
    reset_count = 0;
    memset(event_log, 0, sizeof(event_log));
    mwdt_init(&wdt, mock_clock);
    mwdt_set_timeout_cb(&wdt, on_timeout, NULL);
    mwdt_set_reset_cb(&wdt, on_reset, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Init
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_init) {
    reset_all();
    ASSERT_EQ(0, mwdt_task_count(&wdt));
    ASSERT_EQ(0, (int)mwdt_check_count(&wdt));
    ASSERT_EQ(0, (int)mwdt_timeout_count(&wdt));
    ASSERT_TRUE(mwdt_all_ok(&wdt));
}

TEST(test_init_null) {
    ASSERT_EQ(MWDT_ERR_NULL, mwdt_init(NULL, mock_clock));
    ASSERT_EQ(MWDT_ERR_NULL, mwdt_init(&wdt, NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Registration
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_register) {
    reset_all();
    int idx = mwdt_register(&wdt, "sensor", 5000, 3, false);
    ASSERT_EQ(0, idx);
    ASSERT_EQ(1, mwdt_task_count(&wdt));

    const mwdt_task_t *t = mwdt_task_at(&wdt, 0);
    ASSERT_TRUE(t != NULL);
    ASSERT_STR_EQ("sensor", t->name);
    ASSERT_EQ(5000, (int)t->deadline_ms);
    ASSERT_EQ(3, t->max_misses);
    ASSERT_TRUE(t->enabled);
    ASSERT_EQ(MWDT_TASK_OK, (int)t->state);
}

TEST(test_register_multiple) {
    reset_all();
    mwdt_register(&wdt, "sensor", 5000, 3, false);
    mwdt_register(&wdt, "mqtt", 10000, 2, true);
    mwdt_register(&wdt, "led", 1000, 0, false);
    ASSERT_EQ(3, mwdt_task_count(&wdt));
}

TEST(test_register_null) {
    reset_all();
    ASSERT_EQ(MWDT_ERR_NULL, mwdt_register(NULL, "x", 1000, 1, false));
    ASSERT_EQ(MWDT_ERR_NULL, mwdt_register(&wdt, NULL, 1000, 1, false));
}

TEST(test_register_zero_deadline) {
    reset_all();
    ASSERT_EQ(MWDT_ERR_INVALID, mwdt_register(&wdt, "bad", 0, 1, false));
}

TEST(test_register_full) {
    reset_all();
    for (int i = 0; i < MWDT_MAX_TASKS; i++) {
        ASSERT_GE(mwdt_register(&wdt, "t", 1000, 1, false), 0);
    }
    ASSERT_EQ(MWDT_ERR_FULL, mwdt_register(&wdt, "over", 1000, 1, false));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Kick
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_kick) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 5000, 3, false);

    mock_time += 3000;
    ASSERT_EQ(MWDT_OK, mwdt_kick(&wdt, (uint8_t)idx));
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)idx));
}

TEST(test_kick_by_name) {
    reset_all();
    mwdt_register(&wdt, "sensor", 5000, 3, false);

    mock_time += 3000;
    ASSERT_EQ(MWDT_OK, mwdt_kick_by_name(&wdt, "sensor"));
}

TEST(test_kick_by_name_not_found) {
    reset_all();
    ASSERT_EQ(MWDT_ERR_NOT_FOUND, mwdt_kick_by_name(&wdt, "nonexistent"));
}

TEST(test_kick_resets_miss_count) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 1000, 5, false);

    /* Let it miss once */
    mock_time += 1500;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)idx));

    /* Kick resets */
    mwdt_kick(&wdt, (uint8_t)idx);
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)idx));
    ASSERT_EQ(0, wdt.tasks[idx].miss_count);
}

TEST(test_kick_null) {
    ASSERT_EQ(MWDT_ERR_NULL, mwdt_kick(NULL, 0));
    reset_all();
    ASSERT_EQ(MWDT_ERR_NOT_FOUND, mwdt_kick(&wdt, 99));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Check — no timeout
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_check_all_ok) {
    reset_all();
    mwdt_register(&wdt, "a", 5000, 3, false);
    mwdt_register(&wdt, "b", 10000, 3, false);

    mock_time += 2000;
    int timed_out = mwdt_check(&wdt);
    ASSERT_EQ(0, timed_out);
    ASSERT_TRUE(mwdt_all_ok(&wdt));
    ASSERT_EQ(0, event_count);
    ASSERT_EQ(1, (int)mwdt_check_count(&wdt));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Check — LATE
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_check_late) {
    reset_all();
    int idx = mwdt_register(&wdt, "sensor", 2000, 3, false);

    /* Exceed deadline */
    mock_time += 2500;
    int timed_out = mwdt_check(&wdt);
    ASSERT_EQ(1, timed_out);
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)idx));
    ASSERT_FALSE(mwdt_all_ok(&wdt));

    /* Callback fired */
    ASSERT_EQ(1, event_count);
    ASSERT_STR_EQ("sensor", event_log[0].name);
    ASSERT_EQ(MWDT_TASK_LATE, (int)event_log[0].state);
    ASSERT_EQ(MWDT_TASK_OK, (int)event_log[0].prev_state);
    ASSERT_EQ(2000, (int)event_log[0].deadline_ms);
    ASSERT_EQ(2500, (int)event_log[0].elapsed_ms);
}

TEST(test_check_late_no_duplicate_event) {
    reset_all();
    mwdt_register(&wdt, "sensor", 2000, 5, false);

    /* First check — goes LATE */
    mock_time += 2500;
    mwdt_check(&wdt);
    ASSERT_EQ(1, event_count);

    /* Second check — still LATE, same miss count → no new event */
    mock_time += 100;
    mwdt_check(&wdt);
    ASSERT_EQ(1, event_count);  /* no duplicate */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Check — STARVED
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_check_starved) {
    reset_all();
    int idx = mwdt_register(&wdt, "sensor", 1000, 3, false);

    /* Miss 3 deadlines → STARVED */
    mock_time += 3500;
    mwdt_check(&wdt);

    ASSERT_EQ(MWDT_TASK_STARVED, mwdt_task_state(&wdt, (uint8_t)idx));
    ASSERT_EQ(0, reset_count);  /* auto_reset = false */
}

TEST(test_check_starved_auto_reset) {
    reset_all();
    mwdt_register(&wdt, "critical", 1000, 2, true);  /* auto_reset = true */

    mock_time += 2500;
    mwdt_check(&wdt);

    ASSERT_EQ(1, reset_count);  /* reset_fn called */
}

TEST(test_check_starved_no_reset_fn) {
    reset_all();
    wdt.reset_fn = NULL;  /* no reset function */
    mwdt_register(&wdt, "task", 1000, 2, true);

    mock_time += 2500;
    mwdt_check(&wdt);
    /* Should not crash, just transitions to STARVED */
    ASSERT_EQ(MWDT_TASK_STARVED, mwdt_task_state(&wdt, 0));
    ASSERT_EQ(0, reset_count);
}

TEST(test_late_to_starved_transition) {
    reset_all();
    mwdt_register(&wdt, "task", 1000, 3, false);

    /* 1 miss → LATE */
    mock_time += 1500;
    mwdt_check(&wdt);
    ASSERT_EQ(1, event_count);
    ASSERT_EQ(MWDT_TASK_LATE, (int)event_log[0].state);

    /* 3 misses → STARVED */
    mock_time = 1000 + 3500;
    mwdt_check(&wdt);
    ASSERT_EQ(2, event_count);
    ASSERT_EQ(MWDT_TASK_STARVED, (int)event_log[1].state);
    ASSERT_EQ(MWDT_TASK_LATE, (int)event_log[1].prev_state);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: max_misses = 0 (warn only, never starve)
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_max_misses_zero_never_starves) {
    reset_all();
    int idx = mwdt_register(&wdt, "non_critical", 1000, 0, false);

    /* Miss 100 deadlines */
    mock_time += 100500;
    mwdt_check(&wdt);

    /* Should be LATE, never STARVED */
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)idx));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Enable / Disable
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_disable_skip) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 1000, 3, false);
    mwdt_enable(&wdt, (uint8_t)idx, false);

    mock_time += 5000;
    mwdt_check(&wdt);
    ASSERT_EQ(0, event_count);
    ASSERT_TRUE(mwdt_all_ok(&wdt));
}

TEST(test_reenable_resets) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 1000, 3, false);

    /* Make it late */
    mock_time += 1500;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)idx));

    /* Disable + re-enable resets state */
    mwdt_enable(&wdt, (uint8_t)idx, false);
    mwdt_enable(&wdt, (uint8_t)idx, true);
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)idx));
    ASSERT_EQ(0, wdt.tasks[idx].miss_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Recovery via kick
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_recovery) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 2000, 3, false);

    /* Go LATE */
    mock_time += 2500;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)idx));

    /* Kick → back to OK */
    mwdt_kick(&wdt, (uint8_t)idx);
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)idx));
    ASSERT_TRUE(mwdt_all_ok(&wdt));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Remaining
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_remaining) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 5000, 3, false);

    mock_time += 2000;
    uint32_t rem = mwdt_remaining(&wdt, (uint8_t)idx);
    ASSERT_EQ(3000, (int)rem);
}

TEST(test_remaining_expired) {
    reset_all();
    int idx = mwdt_register(&wdt, "task", 2000, 3, false);

    mock_time += 3000;
    ASSERT_EQ(0, (int)mwdt_remaining(&wdt, (uint8_t)idx));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Find
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_find) {
    reset_all();
    mwdt_register(&wdt, "alpha", 1000, 1, false);
    mwdt_register(&wdt, "beta", 2000, 1, false);
    mwdt_register(&wdt, "gamma", 3000, 1, false);

    ASSERT_EQ(0, mwdt_find(&wdt, "alpha"));
    ASSERT_EQ(1, mwdt_find(&wdt, "beta"));
    ASSERT_EQ(2, mwdt_find(&wdt, "gamma"));
    ASSERT_EQ(-1, mwdt_find(&wdt, "delta"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Multiple tasks
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_multiple_tasks_mixed) {
    reset_all();
    int fast = mwdt_register(&wdt, "fast", 1000, 3, false);
    int slow = mwdt_register(&wdt, "slow", 10000, 3, false);

    /* Fast misses, slow still ok */
    mock_time += 1500;
    mwdt_check(&wdt);

    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)fast));
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)slow));
    ASSERT_FALSE(mwdt_all_ok(&wdt));
    ASSERT_EQ(1, event_count);
    ASSERT_STR_EQ("fast", event_log[0].name);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Full scenario
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_full_scenario) {
    reset_all();

    int sensor = mwdt_register(&wdt, "sensor_task", 2000, 3, false);
    int mqtt   = mwdt_register(&wdt, "mqtt_task",   5000, 2, true);

    /* T+1s: all ok */
    mock_time += 1000;
    mwdt_check(&wdt);
    ASSERT_TRUE(mwdt_all_ok(&wdt));

    /* T+1.5s: kick sensor */
    mock_time += 500;
    mwdt_kick(&wdt, (uint8_t)sensor);

    /* T+3s: sensor ok (kicked at 2.5s, deadline 2s → next at 4.5s) */
    mock_time += 1000;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)sensor));
    ASSERT_EQ(MWDT_TASK_OK, mwdt_task_state(&wdt, (uint8_t)mqtt));

    /* T+7s: mqtt goes LATE (registered at 1s, deadline 5s, no kick) */
    mock_time = 1000 + 6500;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, (uint8_t)mqtt));

    /* T+12s: mqtt STARVED (2 misses, auto_reset) */
    mock_time = 1000 + 11500;
    mwdt_check(&wdt);
    ASSERT_EQ(MWDT_TASK_STARVED, mwdt_task_state(&wdt, (uint8_t)mqtt));
    ASSERT_EQ(1, reset_count);  /* reset triggered */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Edge cases and strings
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_check_null) {
    ASSERT_EQ(0, mwdt_check(NULL));
}

TEST(test_query_null) {
    ASSERT_EQ(0, mwdt_task_count(NULL));
    ASSERT_TRUE(mwdt_task_at(NULL, 0) == NULL);
    ASSERT_EQ(-1, mwdt_find(NULL, "x"));
    ASSERT_EQ(MWDT_TASK_DISABLED, (int)mwdt_task_state(NULL, 0));
    ASSERT_TRUE(mwdt_all_ok(NULL));
    ASSERT_EQ(0, (int)mwdt_remaining(NULL, 0));
    ASSERT_EQ(0, (int)mwdt_timeout_count(NULL));
    ASSERT_EQ(0, (int)mwdt_check_count(NULL));
}

TEST(test_err_str) {
    ASSERT_STR_EQ("ok",              mwdt_err_str(MWDT_OK));
    ASSERT_STR_EQ("null pointer",    mwdt_err_str(MWDT_ERR_NULL));
    ASSERT_STR_EQ("task table full", mwdt_err_str(MWDT_ERR_FULL));
    ASSERT_STR_EQ("task not found",  mwdt_err_str(MWDT_ERR_NOT_FOUND));
    ASSERT_STR_EQ("unknown error",   mwdt_err_str((mwdt_err_t)99));
}

TEST(test_state_str) {
    ASSERT_STR_EQ("OK",       mwdt_task_state_str(MWDT_TASK_OK));
    ASSERT_STR_EQ("LATE",     mwdt_task_state_str(MWDT_TASK_LATE));
    ASSERT_STR_EQ("STARVED",  mwdt_task_state_str(MWDT_TASK_STARVED));
    ASSERT_STR_EQ("DISABLED", mwdt_task_state_str(MWDT_TASK_DISABLED));
}

TEST(test_no_timeout_callback) {
    reset_all();
    wdt.timeout_fn = NULL;
    mwdt_register(&wdt, "task", 1000, 3, false);

    mock_time += 1500;
    mwdt_check(&wdt);
    /* Should not crash, state still updates */
    ASSERT_EQ(MWDT_TASK_LATE, mwdt_task_state(&wdt, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== microwdt test suite ===\n\n");

    printf("[Init]\n");
    RUN_TEST(test_init);
    RUN_TEST(test_init_null);

    printf("\n[Registration]\n");
    RUN_TEST(test_register);
    RUN_TEST(test_register_multiple);
    RUN_TEST(test_register_null);
    RUN_TEST(test_register_zero_deadline);
    RUN_TEST(test_register_full);

    printf("\n[Kick]\n");
    RUN_TEST(test_kick);
    RUN_TEST(test_kick_by_name);
    RUN_TEST(test_kick_by_name_not_found);
    RUN_TEST(test_kick_resets_miss_count);
    RUN_TEST(test_kick_null);

    printf("\n[Check - Healthy]\n");
    RUN_TEST(test_check_all_ok);

    printf("\n[Check - LATE]\n");
    RUN_TEST(test_check_late);
    RUN_TEST(test_check_late_no_duplicate_event);

    printf("\n[Check - STARVED]\n");
    RUN_TEST(test_check_starved);
    RUN_TEST(test_check_starved_auto_reset);
    RUN_TEST(test_check_starved_no_reset_fn);
    RUN_TEST(test_late_to_starved_transition);

    printf("\n[Max Misses Zero]\n");
    RUN_TEST(test_max_misses_zero_never_starves);

    printf("\n[Enable/Disable]\n");
    RUN_TEST(test_disable_skip);
    RUN_TEST(test_reenable_resets);

    printf("\n[Recovery]\n");
    RUN_TEST(test_recovery);

    printf("\n[Remaining]\n");
    RUN_TEST(test_remaining);
    RUN_TEST(test_remaining_expired);

    printf("\n[Find]\n");
    RUN_TEST(test_find);

    printf("\n[Multiple Tasks]\n");
    RUN_TEST(test_multiple_tasks_mixed);

    printf("\n[Full Scenario]\n");
    RUN_TEST(test_full_scenario);

    printf("\n[Edge Cases]\n");
    RUN_TEST(test_check_null);
    RUN_TEST(test_query_null);
    RUN_TEST(test_err_str);
    RUN_TEST(test_state_str);
    RUN_TEST(test_no_timeout_callback);

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
