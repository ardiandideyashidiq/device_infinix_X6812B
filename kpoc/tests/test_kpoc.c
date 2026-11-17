#include "harness.h"
#include "logo_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures_total;
static int checks_total;

static void run(const char *name, void (*fn)(void));

static void t_ctx_init_rejects_null(void);
static void t_ctx_init_arms_state(void);
static void t_ctx_init_unwinds_on_failure(void);
static void t_light_failure_is_reported(void);
static void t_light_success_is_not_an_error(void);
static void t_light_skipped_until_ready(void);

int main(void)
{
    printf("kpoc host tests\n");

    run("ctx: null arguments rejected", t_ctx_init_rejects_null);
    run("ctx: initial state is armed", t_ctx_init_arms_state);
    run("ctx: partial mutex failure unwinds", t_ctx_init_unwinds_on_failure);
    run("light: a failed set is reported", t_light_failure_is_reported);
    run("light: a successful set is not an error", t_light_success_is_not_an_error);
    run("light: no call before the HAL is ready", t_light_skipped_until_ready);

    printf("\n%d checks, %d failures\n", checks_total, failures_total);
    return failures_total == 0 ? 0 : 1;
}

static void run(const char *name, void (*fn)(void))
{
    int before = g_test.failures;
    int checks_before = g_test.checks;

    test_begin(name);
    fn();
    checks_total += g_test.checks - checks_before;
    failures_total += g_test.failures - before;
    if (g_test.failures == before)
        printf("ok\n");
}

/* Threads ops: real pthread wrappers, so the init path runs for real
 * rather than against a hand-written fake.
 */
static int    g_mutex_init_calls;
static int    g_mutex_destroy_calls;
static int    g_fail_mutex_at;   /* 1-based index of the mutex init to fail */

typedef struct { int inited; } FakeMutex;
typedef struct { int inited; } FakeCond;

static int fm_init(FakeMutex *m)
{
    g_mutex_init_calls++;
    if (g_fail_mutex_at && g_mutex_init_calls == g_fail_mutex_at)
        return -1;
    m->inited = 1;
    return 0;
}

static int fm_destroy(FakeMutex *m)
{
    if (!m->inited) {
        test_fail("destroy of a mutex that was never initialised");
        return -1;
    }
    m->inited = 0;
    g_mutex_destroy_calls++;
    return 0;
}
static int fm_lock(FakeMutex *m) { (void)m; return 0; }
static int fm_unlock(FakeMutex *m) { (void)m; return 0; }
static int fc_init(FakeCond *c) { c->inited = 1; return 0; }
static int fc_destroy(FakeCond *c) { c->inited = 0; return 0; }
static int fc_wait(FakeCond *c, FakeMutex *m) { (void)c; (void)m; return 0; }
static int fc_signal(FakeCond *c) { (void)c; return 0; }

static void threads_init(KpocThreadOps *t)
{
    memset(t, 0, sizeof(*t));
    t->mutex_init = (int (*)(void *))fm_init;
    t->mutex_destroy = (int (*)(void *))fm_destroy;
    t->mutex_lock = (int (*)(void *))fm_lock;
    t->mutex_unlock = (int (*)(void *))fm_unlock;
    t->cond_init = (int (*)(void *))fc_init;
    t->cond_destroy = (int (*)(void *))fc_destroy;
    t->cond_wait = (int (*)(void *, void *))fc_wait;
    t->cond_signal = (int (*)(void *))fc_signal;
}

static void t_ctx_init_rejects_null(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    CHECK_EQ_I(-1, kpoc_ctx_init(NULL, &f.ops, &t));
    CHECK_EQ_I(-1, kpoc_ctx_init(&ctx, NULL, &t));
    CHECK_EQ_I(-1, kpoc_ctx_init(&ctx, &f.ops, NULL));
}

static void t_ctx_init_arms_state(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    g_mutex_init_calls = 0;
    g_fail_mutex_at = 0;

    CHECK_EQ_I(0, kpoc_ctx_init(&ctx, &f.ops, &t));
    CHECK(ctx.ops == &f.ops);
    CHECK(ctx.threads == &t);
    CHECK_EQ_I(0, atomic_load(&ctx.light_ready));
    CHECK_EQ_I(0, atomic_load(&ctx.health_ready));
    CHECK_EQ_U(KPOC_VBAT_THRESHOLD_UV, ctx.vbat_threshold_uv);
    /* four mutexes in init: m_main, m_state, m_draw, m_key */
    CHECK_EQ_I(4, g_mutex_init_calls);
}

/* A failure mid-way must release only what it took. If the unwind order is
 * wrong this leaks or destroys an uninitialised mutex.
 */
static void t_ctx_init_unwinds_on_failure(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    g_mutex_init_calls = 0;
    g_mutex_destroy_calls = 0;
    g_fail_mutex_at = 3;         /* m_draw fails; m_main and m_state were taken */

    CHECK_EQ_I(-1, kpoc_ctx_init(&ctx, &f.ops, &t));
    /* Two mutexes succeeded and must be released; nothing else may be touched. */
    CHECK_EQ_I(2, g_mutex_destroy_calls);
}

static int g_light_result;
static unsigned g_light_set_calls;
static uint32_t g_light_type;
static uint32_t g_light_argb;

static int light_set_stub(uint32_t type, uint32_t argb)
{
    g_light_set_calls++;
    g_light_type = type;
    g_light_argb = argb;
    return g_light_result;
}

static void t_light_failure_is_reported(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    g_fail_mutex_at = 0;
    CHECK_EQ_I(0, kpoc_ctx_init(&ctx, &f.ops, &t));

    f.ops.light_set = light_set_stub;
    atomic_store(&ctx.light_ready, 1);
    g_light_result = -13;
    g_light_set_calls = 0;

    f.log_calls = 0;
    f.last_log[0] = '\0';
    kpoc_set_light_brightness(&ctx, 0, 0xff969696u);

    CHECK_EQ_U(1u, g_light_set_calls);
    CHECK_EQ_U(1u, f.log_calls);
    CHECK(strstr(f.last_log, "Failed to set light") != NULL);
}

static void t_light_success_is_not_an_error(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    g_fail_mutex_at = 0;
    CHECK_EQ_I(0, kpoc_ctx_init(&ctx, &f.ops, &t));

    f.ops.light_set = light_set_stub;
    atomic_store(&ctx.light_ready, 1);
    g_light_result = 0;
    g_light_set_calls = 0;

    f.log_calls = 0;
    f.last_log[0] = '\0';
    kpoc_set_light_brightness(&ctx, 0, 0xff969696u);

    CHECK_EQ_U(1u, g_light_set_calls);
    CHECK(strstr(f.last_log, "successfully") != NULL);
    CHECK(strstr(f.last_log, "Failed") == NULL);
    /* the value must be passed through untouched, not re-derived */
    CHECK_EQ_U(0u, g_light_type);
    CHECK_EQ_U(0xff969696u, g_light_argb);
}

/* Before the light HAL is registered the call must be skipped entirely;
 * a sysfs fallback here is what produced the stale -13 log lines.
 */
static void t_light_skipped_until_ready(void)
{
    KpocCtx ctx;
    FakeOps f;
    KpocThreadOps t;

    fake_ops_init(&f);
    threads_init(&t);
    g_fail_mutex_at = 0;
    CHECK_EQ_I(0, kpoc_ctx_init(&ctx, &f.ops, &t));

    f.ops.light_set = light_set_stub;
    atomic_store(&ctx.light_ready, 0);
    g_light_result = 0;
    g_light_set_calls = 0;

    f.log_calls = 0;
    f.last_log[0] = '\0';
    kpoc_set_light_brightness(&ctx, 0, 0xff969696u);

    /* no HAL call at all, and the reason is logged */
    CHECK_EQ_U(0u, g_light_set_calls);
    CHECK(strstr(f.last_log, "No light service") != NULL);
}
