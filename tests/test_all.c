#include "mwdt.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t now_ms;
    unsigned calls;
} clock_ctx_t;

typedef enum {
    CB_MODE_CAPTURE = 0,
    CB_MODE_BUSY_MUTATIONS,
    CB_MODE_OPERATE_OTHER
} cb_mode_t;

typedef struct {
    unsigned calls;
    mwdt_timeout_t last_event;
    const mwdt_timeout_t *last_event_ptr;
    size_t queried_index;
    mwdt_task_state_t queried_state;
    uint32_t queried_miss_count;
    uint32_t queried_transition_count;
    bool queried_all_ok;
    mwdt_err_t kick_result;
    mwdt_err_t disable_result;
    mwdt_err_t register_result;
    mwdt_err_t reinit_result;
    mwdt_err_t nested_check_result;
    mwdt_err_t set_timeout_result;
    mwdt_err_t set_reset_result;
    mwdt_err_t other_result;
} timeout_log_t;

typedef struct {
    unsigned calls;
    mwdt_timeout_t last_event;
    mwdt_err_t mutation_result;
} reset_log_t;

typedef struct {
    clock_ctx_t clock_a;
    clock_ctx_t clock_b;
    timeout_log_t timeout_log;
    reset_log_t reset_log;
    mwdt_t wdt_a;
    mwdt_t wdt_b;
    mwdt_task_t storage_a[300];
    mwdt_task_t storage_b[8];
    cb_mode_t cb_mode;
} fixture_t;

static fixture_t g_fixture;
static unsigned g_tests_run;
static unsigned g_tests_passed;
static unsigned g_tests_failed;
static unsigned g_assertions;

static uint32_t test_clock(void *ctx)
{
    clock_ctx_t *clock_ctx = (clock_ctx_t *)ctx;

    clock_ctx->calls++;
    return clock_ctx->now_ms;
}

static void reset_fixture(void)
{
    memset(&g_fixture, 0, sizeof(g_fixture));
    g_fixture.clock_a.now_ms = 1000U;
    g_fixture.clock_b.now_ms = 2000U;
    g_fixture.cb_mode = CB_MODE_CAPTURE;
}

#define ASSERT_TRUE(expr) \
    do { \
        const int mwdt_assert_value = (expr) ? 1 : 0; \
        g_assertions++; \
        if (!mwdt_assert_value) { \
            printf("FAIL\n  %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
            return false; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) \
    do { \
        const int mwdt_assert_value = (expr) ? 1 : 0; \
        g_assertions++; \
        if (mwdt_assert_value) { \
            printf("FAIL\n  %s:%d: expected false: %s\n", __FILE__, __LINE__, #expr); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_INT(expected, actual) \
    do { \
        const int mwdt_expected_value = (expected); \
        const int mwdt_actual_value = (actual); \
        g_assertions++; \
        if (mwdt_expected_value != mwdt_actual_value) { \
            printf("FAIL\n  %s:%d: expected %d, got %d\n", __FILE__, __LINE__, mwdt_expected_value, mwdt_actual_value); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_U32(expected, actual) \
    do { \
        const uint32_t mwdt_expected_value = (expected); \
        const uint32_t mwdt_actual_value = (actual); \
        g_assertions++; \
        if (mwdt_expected_value != mwdt_actual_value) { \
            printf("FAIL\n  %s:%d: expected %lu, got %lu\n", __FILE__, __LINE__, \
                (unsigned long)mwdt_expected_value, (unsigned long)mwdt_actual_value); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_SIZE(expected, actual) \
    do { \
        const size_t mwdt_expected_value = (expected); \
        const size_t mwdt_actual_value = (actual); \
        g_assertions++; \
        if (mwdt_expected_value != mwdt_actual_value) { \
            printf("FAIL\n  %s:%d: expected %lu, got %lu\n", __FILE__, __LINE__, \
                (unsigned long)mwdt_expected_value, (unsigned long)mwdt_actual_value); \
            return false; \
        } \
    } while (0)

#define ASSERT_PTR_EQ(expected, actual) \
    do { \
        const void *mwdt_expected_value = (expected); \
        const void *mwdt_actual_value = (actual); \
        g_assertions++; \
        if (mwdt_expected_value != mwdt_actual_value) { \
            printf("FAIL\n  %s:%d: expected %p, got %p\n", __FILE__, __LINE__, mwdt_expected_value, mwdt_actual_value); \
            return false; \
        } \
    } while (0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        const char *mwdt_expected_value = (expected); \
        const char *mwdt_actual_value = (actual); \
        g_assertions++; \
        if (strcmp(mwdt_expected_value, mwdt_actual_value) != 0) { \
            printf("FAIL\n  %s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, mwdt_expected_value, mwdt_actual_value); \
            return false; \
        } \
    } while (0)

static mwdt_config_t make_config_a(void)
{
    mwdt_config_t config;

    memset(&config, 0, sizeof(config));
    config.tasks = g_fixture.storage_a;
    config.task_capacity = sizeof(g_fixture.storage_a) / sizeof(g_fixture.storage_a[0]);
    config.clock_fn = test_clock;
    config.clock_ctx = &g_fixture.clock_a;
    return config;
}

static mwdt_config_t make_config_b(void)
{
    mwdt_config_t config;

    memset(&config, 0, sizeof(config));
    config.tasks = g_fixture.storage_b;
    config.task_capacity = sizeof(g_fixture.storage_b) / sizeof(g_fixture.storage_b[0]);
    config.clock_fn = test_clock;
    config.clock_ctx = &g_fixture.clock_b;
    return config;
}

static void timeout_cb(const mwdt_timeout_t *event, void *ctx)
{
    timeout_log_t *log = (timeout_log_t *)ctx;
    mwdt_task_snapshot_t snapshot;
    uint32_t transition_count = 0U;
    bool all_ok = false;

    log->calls++;
    log->last_event = *event;
    log->last_event_ptr = event;
    log->queried_index = event->task_index;
    (void)mwdt_get_task_state(&g_fixture.wdt_a, event->task_index, &log->queried_state);
    (void)mwdt_get_task(&g_fixture.wdt_a, event->task_index, &snapshot);
    (void)mwdt_get_transition_event_count(&g_fixture.wdt_a, &transition_count);
    (void)mwdt_get_all_ok(&g_fixture.wdt_a, &all_ok);
    log->queried_miss_count = snapshot.miss_count;
    log->queried_transition_count = transition_count;
    log->queried_all_ok = all_ok;

    if (g_fixture.cb_mode == CB_MODE_BUSY_MUTATIONS) {
        size_t nested_out = 0U;
        size_t throwaway_index = 0U;
        mwdt_config_t config = make_config_a();

        log->kick_result = mwdt_kick(&g_fixture.wdt_a, event->task_index);
        log->disable_result = mwdt_enable(&g_fixture.wdt_a, event->task_index, false);
        log->register_result = mwdt_register(&g_fixture.wdt_a, "nested", 10U, 1U, false, &throwaway_index);
        log->reinit_result = mwdt_init(&g_fixture.wdt_a, &config);
        log->nested_check_result = mwdt_check(&g_fixture.wdt_a, &nested_out);
        log->set_timeout_result = mwdt_set_timeout_cb(&g_fixture.wdt_a, timeout_cb, ctx);
        log->set_reset_result = mwdt_set_reset_cb(&g_fixture.wdt_a, NULL, NULL);
    } else if (g_fixture.cb_mode == CB_MODE_OPERATE_OTHER) {
        log->other_result = mwdt_kick(&g_fixture.wdt_b, 0U);
    }
}

static void reset_cb(const mwdt_timeout_t *event, void *ctx)
{
    reset_log_t *log = (reset_log_t *)ctx;

    log->calls++;
    log->last_event = *event;
    log->mutation_result = mwdt_kick(&g_fixture.wdt_a, event->task_index);
}

static bool init_watchdogs_with_callbacks(void)
{
    mwdt_config_t config_a = make_config_a();
    mwdt_config_t config_b = make_config_b();

    config_a.timeout_fn = timeout_cb;
    config_a.timeout_ctx = &g_fixture.timeout_log;
    config_a.reset_fn = reset_cb;
    config_a.reset_ctx = &g_fixture.reset_log;

    if (mwdt_init(&g_fixture.wdt_a, &config_a) != MWDT_OK) {
        return false;
    }
    if (mwdt_init(&g_fixture.wdt_b, &config_b) != MWDT_OK) {
        return false;
    }
    return true;
}

static bool test_init_and_config_copy(void)
{
    mwdt_t zeroed = {0};
    mwdt_task_t tiny_storage[1];
    mwdt_config_t invalid = {0};
    mwdt_config_t config = make_config_a();
    mwdt_config_t other = make_config_b();
    const char task_name[] = "solo";
    size_t index = 0U;
    mwdt_task_snapshot_t snapshot;

    reset_fixture();

    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_init(NULL, &config));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_init(&g_fixture.wdt_a, NULL));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_init(&g_fixture.wdt_a, &invalid));

    invalid.tasks = tiny_storage;
    invalid.task_capacity = 1U;
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_init(&g_fixture.wdt_a, &invalid));

    config.task_capacity = 1U;
    config.tasks = tiny_storage;
    config.timeout_fn = timeout_cb;
    config.timeout_ctx = &g_fixture.timeout_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));

    config.clock_ctx = &other.clock_ctx;
    config.timeout_ctx = NULL;

    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, task_name, 10U, 1U, false, &index));
    ASSERT_EQ_SIZE(0U, index);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_U32(1000U, snapshot.last_kick_ms);
    ASSERT_PTR_EQ(task_name, snapshot.name);
    ASSERT_EQ_INT(MWDT_ERR_UNINITIALIZED, mwdt_get_task_count(&zeroed, &index));
    return true;
}

static bool test_capacity_duplicate_and_large_table(void)
{
    size_t index = 0U;
    size_t count = 0U;
    mwdt_config_t config = make_config_a();
    char names[260][16];
    unsigned i;

    reset_fixture();
    config.task_capacity = 260U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "dup", 10U, 1U, false, &index));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_register(&g_fixture.wdt_a, "dup", 10U, 1U, false, &index));

    for (i = 0U; i < 259U; ++i) {
        (void)snprintf(names[i], sizeof(names[i]), "t%u", i);
        ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, names[i], 10U, UINT32_MAX, false, &index));
        ASSERT_EQ_SIZE((size_t)(i + 1U), index);
    }

    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_count(&g_fixture.wdt_a, &count));
    ASSERT_EQ_SIZE(260U, count);
    ASSERT_EQ_INT(MWDT_ERR_FULL, mwdt_register(&g_fixture.wdt_a, "overflow", 10U, 1U, false, &index));
    return true;
}

static bool test_register_validation(void)
{
    size_t index = 99U;
    mwdt_t zeroed = {0};
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_register(&g_fixture.wdt_a, "x", 10U, 1U, false, NULL));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_register(&g_fixture.wdt_a, NULL, 10U, 1U, false, &index));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_register(&g_fixture.wdt_a, "", 10U, 1U, false, &index));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_register(&g_fixture.wdt_a, "x", 0U, 1U, false, &index));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_register(&g_fixture.wdt_a, "x", 10U, 0U, true, &index));
    ASSERT_EQ_INT(MWDT_ERR_INVALID, mwdt_register(&g_fixture.wdt_a, "x", 10U, 1U, true, &index));
    ASSERT_EQ_INT(MWDT_ERR_UNINITIALIZED, mwdt_register(&zeroed, "x", 10U, 1U, false, &index));
    return true;
}

static bool test_time_boundaries_and_wrap(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    mwdt_task_state_t state = MWDT_TASK_DISABLED;
    uint32_t remaining = 0U;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "edge", 100U, 3U, false, &index));

    g_fixture.clock_a.now_ms = 1099U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_SIZE(0U, timed_out);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_remaining(&g_fixture.wdt_a, index, &remaining));
    ASSERT_EQ_U32(1U, remaining);

    g_fixture.clock_a.now_ms = 1100U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_SIZE(1U, timed_out);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_LATE, state);

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    g_fixture.clock_a.now_ms = UINT32_MAX - 20U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "wrap", 10U, 3U, false, &index));
    g_fixture.clock_a.now_ms = 24U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, state);
    return true;
}

static bool test_large_miss_counts_and_direct_starve(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    mwdt_task_snapshot_t snapshot;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "huge", 1U, UINT32_MAX, false, &index));

    g_fixture.clock_a.now_ms = 1255U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_U32(255U, snapshot.miss_count);
    ASSERT_EQ_INT(MWDT_TASK_LATE, snapshot.state);

    g_fixture.clock_a.now_ms = 1256U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_U32(256U, snapshot.miss_count);

    g_fixture.clock_a.now_ms = UINT32_MAX;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_U32(UINT32_MAX - 1000U, snapshot.miss_count);

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "direct", 100U, 2U, false, &index));
    g_fixture.clock_a.now_ms = 1300U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, snapshot.state);
    ASSERT_EQ_U32(3U, snapshot.miss_count);
    return true;
}

static bool test_starved_monotonicity_and_recovery(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    mwdt_task_state_t state = MWDT_TASK_DISABLED;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "task", 100U, 1U, false, &index));
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, state);

    g_fixture.clock_a.now_ms = 1300U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, state);

    ASSERT_EQ_INT(MWDT_OK, mwdt_kick(&g_fixture.wdt_a, index));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_OK, state);
    return true;
}

static bool test_enable_disable_and_disabled_kick(void)
{
    size_t index = 0U;
    uint32_t remaining = 0U;
    mwdt_task_state_t state = MWDT_TASK_OK;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "task", 100U, 2U, false, &index));
    ASSERT_EQ_INT(MWDT_OK, mwdt_enable(&g_fixture.wdt_a, index, false));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_DISABLED, state);
    ASSERT_EQ_INT(MWDT_ERR_DISABLED, mwdt_get_remaining(&g_fixture.wdt_a, index, &remaining));
    ASSERT_EQ_INT(MWDT_ERR_DISABLED, mwdt_kick(&g_fixture.wdt_a, index));

    g_fixture.clock_a.now_ms = 1500U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_enable(&g_fixture.wdt_a, index, true));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_OK, state);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_remaining(&g_fixture.wdt_a, index, &remaining));
    ASSERT_EQ_U32(100U, remaining);
    return true;
}

static bool test_query_contracts(void)
{
    size_t count = 0U;
    bool all_ok = false;
    uint32_t count_u32 = 0U;
    mwdt_t zeroed = {0};
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_check(NULL, &count));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_get_all_ok(NULL, &all_ok));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_get_check_count(NULL, &count_u32));
    ASSERT_EQ_INT(MWDT_ERR_UNINITIALIZED, mwdt_get_all_ok(&zeroed, &all_ok));

    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_ERR_NULL, mwdt_get_task_count(&g_fixture.wdt_a, NULL));
    ASSERT_EQ_INT(MWDT_ERR_NOT_FOUND, mwdt_get_task_state(&g_fixture.wdt_a, 0U, (mwdt_task_state_t *)&count));
    ASSERT_EQ_INT(MWDT_ERR_NOT_FOUND, mwdt_get_remaining(&g_fixture.wdt_a, 0U, &count_u32));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_all_ok(&g_fixture.wdt_a, &all_ok));
    ASSERT_TRUE(all_ok);
    return true;
}

static bool test_callback_observes_committed_state(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    uint32_t transition_count = 0U;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    config.timeout_fn = timeout_cb;
    config.timeout_ctx = &g_fixture.timeout_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "sensor", 100U, 2U, false, &index));
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_SIZE(1U, timed_out);
    ASSERT_EQ_INT(1, (int)g_fixture.timeout_log.calls);
    ASSERT_EQ_INT(MWDT_TASK_STARVED, g_fixture.timeout_log.last_event.state);
    ASSERT_EQ_INT(MWDT_TASK_OK, g_fixture.timeout_log.last_event.prev_state);
    ASSERT_EQ_U32(2U, g_fixture.timeout_log.last_event.miss_count);
    ASSERT_EQ_INT(MWDT_TASK_STARVED, g_fixture.timeout_log.queried_state);
    ASSERT_EQ_U32(2U, g_fixture.timeout_log.queried_miss_count);
    ASSERT_EQ_U32(1U, g_fixture.timeout_log.queried_transition_count);
    ASSERT_FALSE(g_fixture.timeout_log.queried_all_ok);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_transition_event_count(&g_fixture.wdt_a, &transition_count));
    ASSERT_EQ_U32(1U, transition_count);
    return true;
}

static bool test_callback_same_instance_busy(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    size_t count = 0U;
    mwdt_task_snapshot_t snapshot;

    reset_fixture();
    ASSERT_TRUE(init_watchdogs_with_callbacks());
    g_fixture.cb_mode = CB_MODE_BUSY_MUTATIONS;
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "busy", 100U, 3U, false, &index));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_count(&g_fixture.wdt_a, &count));
    ASSERT_EQ_SIZE(1U, count);

    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.kick_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.disable_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.register_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.reinit_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.nested_check_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.set_timeout_result);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.timeout_log.set_reset_result);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_count(&g_fixture.wdt_a, &count));
    ASSERT_EQ_SIZE(1U, count);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_EQ_INT(MWDT_TASK_LATE, snapshot.state);
    ASSERT_EQ_U32(2U, snapshot.miss_count);
    return true;
}

static bool test_callback_other_instance_allowed(void)
{
    size_t index_a = 0U;
    size_t index_b = 0U;
    size_t timed_out = 0U;
    mwdt_task_snapshot_t snapshot_b;

    reset_fixture();
    ASSERT_TRUE(init_watchdogs_with_callbacks());
    g_fixture.cb_mode = CB_MODE_OPERATE_OTHER;
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "a", 100U, 2U, false, &index_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_b, "b", 100U, 2U, false, &index_b));
    g_fixture.clock_b.now_ms = 2500U;

    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, g_fixture.timeout_log.other_result);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_b, index_b, &snapshot_b));
    ASSERT_EQ_U32(2500U, snapshot_b.last_kick_ms);
    return true;
}

static bool test_reset_latch_and_repeat_checks(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    bool reset_requested = false;
    size_t reset_index = 0U;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    config.timeout_fn = timeout_cb;
    config.timeout_ctx = &g_fixture.timeout_log;
    config.reset_fn = reset_cb;
    config.reset_ctx = &g_fixture.reset_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "critical", 100U, 1U, true, &index));

    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(1, (int)g_fixture.reset_log.calls);
    ASSERT_EQ_INT(MWDT_ERR_BUSY, g_fixture.reset_log.mutation_result);
    ASSERT_EQ_INT(MWDT_OK, mwdt_reset_is_requested(&g_fixture.wdt_a, &reset_requested));
    ASSERT_TRUE(reset_requested);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_reset_trigger_task(&g_fixture.wdt_a, &reset_index));
    ASSERT_EQ_SIZE(index, reset_index);
    ASSERT_EQ_INT(MWDT_ERR_RESET_LATCHED, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_ERR_RESET_LATCHED, mwdt_kick(&g_fixture.wdt_a, index));
    ASSERT_EQ_INT(1, (int)g_fixture.reset_log.calls);
    return true;
}

static bool test_two_starved_auto_reset_tasks_only_one_reset(void)
{
    size_t first = 0U;
    size_t second = 0U;
    size_t timed_out = 0U;
    mwdt_task_state_t state = MWDT_TASK_OK;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    config.reset_fn = reset_cb;
    config.reset_ctx = &g_fixture.reset_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "first", 100U, 1U, true, &first));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "second", 100U, 1U, true, &second));
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(1, (int)g_fixture.reset_log.calls);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, second, &state));
    ASSERT_EQ_INT(MWDT_TASK_OK, state);
    return true;
}

static bool test_clear_reset_latch_policy(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    bool reset_requested = true;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    config.reset_fn = reset_cb;
    config.reset_ctx = &g_fixture.reset_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "critical", 100U, 1U, true, &index));
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_ERR_STATE, mwdt_clear_reset_request(&g_fixture.wdt_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_enable(&g_fixture.wdt_a, index, false));
    ASSERT_EQ_INT(MWDT_OK, mwdt_clear_reset_request(&g_fixture.wdt_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_reset_is_requested(&g_fixture.wdt_a, &reset_requested));
    ASSERT_FALSE(reset_requested);
    ASSERT_EQ_INT(MWDT_OK, mwdt_enable(&g_fixture.wdt_a, index, true));
    return true;
}

static bool test_no_timeout_callback_still_updates(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    uint32_t transition_count = 0U;
    mwdt_task_state_t state = MWDT_TASK_OK;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "quiet", 100U, 2U, false, &index));
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index, &state));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, state);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_transition_event_count(&g_fixture.wdt_a, &transition_count));
    ASSERT_EQ_U32(1U, transition_count);
    return true;
}

static bool test_counters_saturate(void)
{
    size_t index = 0U;
    size_t timed_out = 0U;
    uint32_t check_count = 0U;
    uint32_t transition_count = 0U;
    uint32_t reset_count = 0U;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    config.reset_fn = reset_cb;
    config.reset_ctx = &g_fixture.reset_log;
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "critical", 100U, 1U, true, &index));
    g_fixture.wdt_a.check_count = UINT32_MAX - 1U;
    g_fixture.wdt_a.transition_event_count = UINT32_MAX - 1U;
    g_fixture.wdt_a.reset_request_count = UINT32_MAX - 1U;
    g_fixture.clock_a.now_ms = 1200U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_check_count(&g_fixture.wdt_a, &check_count));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_transition_event_count(&g_fixture.wdt_a, &transition_count));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_reset_request_count(&g_fixture.wdt_a, &reset_count));
    ASSERT_EQ_U32(UINT32_MAX, check_count);
    ASSERT_EQ_U32(UINT32_MAX, transition_count);
    ASSERT_EQ_U32(UINT32_MAX, reset_count);
    return true;
}

static bool test_two_instances_and_context_separation(void)
{
    size_t index_a = 0U;
    size_t index_b = 0U;
    size_t timed_out = 0U;
    mwdt_task_state_t state_a = MWDT_TASK_OK;
    mwdt_task_state_t state_b = MWDT_TASK_OK;
    mwdt_config_t config_a = make_config_a();
    mwdt_config_t config_b = make_config_b();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_b, &config_b));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, "a", 100U, 2U, false, &index_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_b, "b", 100U, 2U, false, &index_b));
    g_fixture.clock_a.now_ms = 1200U;
    g_fixture.clock_b.now_ms = 2050U;
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_a, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_check(&g_fixture.wdt_b, &timed_out));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_a, index_a, &state_a));
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task_state(&g_fixture.wdt_b, index_b, &state_b));
    ASSERT_EQ_INT(MWDT_TASK_STARVED, state_a);
    ASSERT_EQ_INT(MWDT_TASK_OK, state_b);
    ASSERT_TRUE(g_fixture.clock_a.calls > 0U);
    ASSERT_TRUE(g_fixture.clock_b.calls > 0U);
    return true;
}

static bool test_name_borrowing_and_find(void)
{
    char task_name[] = "borrowed";
    size_t index = 0U;
    size_t found = 0U;
    mwdt_task_snapshot_t snapshot;
    mwdt_config_t config = make_config_a();

    reset_fixture();
    ASSERT_EQ_INT(MWDT_OK, mwdt_init(&g_fixture.wdt_a, &config));
    ASSERT_EQ_INT(MWDT_OK, mwdt_register(&g_fixture.wdt_a, task_name, 100U, 2U, false, &index));
    ASSERT_EQ_INT(MWDT_OK, mwdt_find(&g_fixture.wdt_a, "borrowed", &found));
    ASSERT_EQ_SIZE(index, found);
    ASSERT_EQ_INT(MWDT_OK, mwdt_get_task(&g_fixture.wdt_a, index, &snapshot));
    ASSERT_PTR_EQ(task_name, snapshot.name);
    return true;
}

typedef bool (*test_fn_t)(void);

static void run_test(const char *name, test_fn_t fn)
{
    bool passed;

    g_tests_run++;
    printf("  %-46s ", name);
    passed = fn();
    if (passed) {
        g_tests_passed++;
        printf("PASS\n");
    } else {
        g_tests_failed++;
    }
}

int main(void)
{
    printf("microwdt runtime tests\n\n");

    run_test("test_init_and_config_copy", test_init_and_config_copy);
    run_test("test_capacity_duplicate_and_large_table", test_capacity_duplicate_and_large_table);
    run_test("test_register_validation", test_register_validation);
    run_test("test_time_boundaries_and_wrap", test_time_boundaries_and_wrap);
    run_test("test_large_miss_counts_and_direct_starve", test_large_miss_counts_and_direct_starve);
    run_test("test_starved_monotonicity_and_recovery", test_starved_monotonicity_and_recovery);
    run_test("test_enable_disable_and_disabled_kick", test_enable_disable_and_disabled_kick);
    run_test("test_query_contracts", test_query_contracts);
    run_test("test_callback_observes_committed_state", test_callback_observes_committed_state);
    run_test("test_callback_same_instance_busy", test_callback_same_instance_busy);
    run_test("test_callback_other_instance_allowed", test_callback_other_instance_allowed);
    run_test("test_reset_latch_and_repeat_checks", test_reset_latch_and_repeat_checks);
    run_test("test_two_starved_auto_reset_tasks_only_one_reset", test_two_starved_auto_reset_tasks_only_one_reset);
    run_test("test_clear_reset_latch_policy", test_clear_reset_latch_policy);
    run_test("test_no_timeout_callback_still_updates", test_no_timeout_callback_still_updates);
    run_test("test_counters_saturate", test_counters_saturate);
    run_test("test_two_instances_and_context_separation", test_two_instances_and_context_separation);
    run_test("test_name_borrowing_and_find", test_name_borrowing_and_find);

    printf("\nresults: %u/%u passed, %u failed, %u assertions\n",
        g_tests_passed,
        g_tests_run,
        g_tests_failed,
        g_assertions);

    return g_tests_failed == 0U ? 0 : 1;
}
