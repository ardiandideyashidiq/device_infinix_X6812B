/*
 * lifecycle.c - thread bodies and the top-level daemon sequence.
 *
 * Owns the uevent, draw and key threads plus the decision of when to hand
 * over to Android, power off, or exit for an alarm. The ordering here is
 * load-bearing: the panel must be unblanked before the first draw, and the
 * alarm worker must run before any exit path.
 */

#include "kpoc.h"

#define KPOC_EV_KEY 1u

void kpoc_exit(KpocCtx *ctx, int reason)
{
    kpoc_log(ctx, "exit_charger: %d \n", reason);
    ctx->ops->reboot(kpoc_reboot_command(reason));
    for (;;) { }                               
}

static void *uevent_thread_fn(void *arg)
{
    KpocCtx *c = (KpocCtx *)arg;
    for (;;) {
        kpoc_uevent_worker(c);
        if (c->uevent_failed)
            break;
    }
    return NULL;
}

static void *draw_thread_fn(void *arg)
{
    KpocCtx *c = (KpocCtx *)arg;
    for (;;)
        kpoc_draw_worker(c);
    return NULL;
}

static void *key_thread_fn(void *arg)
{
    KpocCtx *c = (KpocCtx *)arg;
    for (;;)
        kpoc_key_worker(c);
    return NULL;
}

void kpoc_main_sequence(KpocCtx *ctx)
{
    kpoc_set_draw_anim_mode(ctx, 1);
    ctx->ops->set_nice(KPOC_NICE);
    ctx->ops->write_file(KPOC_OOM_PATH, KPOC_OOM_SCORE_ADJ);
    (void)kpoc_health_init(ctx);
    (void)kpoc_lights_init(ctx);
    kpoc_log(ctx, "stop_backlight 1\n");
    kpoc_bootlogo_init(ctx);
    kpoc_alarm_control(ctx);
    kpoc_charging_control(ctx);
    kpoc_log(ctx, "pwrkeys[%d]:%d\n", 0, KPOC_KEYCODE_POWER);
    kpoc_log(ctx, "pwrkeys[%d]:%d\n", 1, KPOC_KEYCODE_ALT);
    kpoc_key_control(ctx);
}

void kpoc_charging_control(KpocCtx *ctx)
{
    if (!kpoc_health_source_available(ctx)) {
        ctx->threads->mutex_lock(ctx->m_state);
        ctx->state = KPOC_CHG_NO_SOURCE;
        ctx->threads->mutex_unlock(ctx->m_state);
        kpoc_lights_chgexit(ctx);
    }

                                                            
    atomic_store(&ctx->in_draw, 0);
    if (ctx->threads->create(KPOC_WORKER_UEVENT, uevent_thread_fn, ctx) != 0) {
        kpoc_log(ctx, "create uevt pthread failed.\n");
        kpoc_exit(ctx, KPOC_EXIT_FATAL);
    }
    atomic_store(&ctx->waiting_for_source, 1);
    if (ctx->threads->create(KPOC_WORKER_DRAW, draw_thread_fn, ctx) != 0) {
        kpoc_log(ctx, "create draw pthread failed.\n");
        kpoc_exit(ctx, KPOC_EXIT_FATAL);
    }
}

void kpoc_long_press_control(KpocCtx *ctx)
{
    if (ctx->threads->create(KPOC_WORKER_KEY, key_thread_fn, ctx) != 0) {
        kpoc_log(ctx, "create key pthread failed.\n");
        kpoc_exit(ctx, KPOC_EXIT_FATAL);
    }
}

void kpoc_key_control(KpocCtx *ctx)
{
    char name[128];
    KpocInputEvent ev;
    int r;

    kpoc_long_press_control(ctx);

    if (ctx->ops->input_scan_begin() != 0) {
        kpoc_log(ctx, "%s(), open_device %s\n", "key_control", KPOC_PATH_INPUT);
        kpoc_exit(ctx, KPOC_EXIT_FATAL);
    }
    while (ctx->ops->input_scan_next(name, sizeof(name)) == 0)
        (void)ctx->ops->input_open(name);
    ctx->ops->input_scan_end();

    for (;;) {
        r = ctx->ops->input_dispatch();
        if (r == KPOC_INPUT_STOP)                                               
            return;
        if (r < 0) {
            kpoc_log(ctx, "epoll_wait returned with errno=%d\n", -r);
            kpoc_exit(ctx, KPOC_EXIT_FATAL);
        }
        if (r == 0) {
            kpoc_log(ctx, "epoll_wait should not return 0 with no timeout\n");
            kpoc_exit(ctx, KPOC_EXIT_FATAL);
        }
        while (ctx->ops->input_next_event(&ev) == 0) {
            if (ev.type != KPOC_EV_KEY)
                continue;
            ctx->threads->mutex_lock(ctx->m_key);
            if (ev.code == KPOC_KEYCODE_POWER || ev.code == KPOC_KEYCODE_ALT) {
                if (ev.value == 1) {                                 
                    atomic_store(&ctx->power_key_down, 1);
                    ++ctx->key_wake_gen;
                    ctx->threads->cond_signal(ctx->c_key);
                } else if (ev.value == 0) {                              
                    atomic_store(&ctx->power_key_down, 0);
                }
            }
            ctx->threads->mutex_unlock(ctx->m_key);
        }
    }
}
