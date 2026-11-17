/*
 * ctx.c - allocation and teardown of the shared daemon context.
 *
 * kpoc_ctx_init() wires the platform ops, creates the sync primitives and
 * seeds the state machine. kpoc_ctx_deinit() must be safe on a partially
 * initialised context, because failure paths call it early.
 */

#include "kpoc.h"
#include <string.h>

int kpoc_ctx_init(KpocCtx *ctx, const KpocOps *ops, const KpocThreadOps *threads)
{
    int stage = 0;

    if (ctx == NULL || ops == NULL || threads == NULL)
        return -1;              

    memset(ctx, 0, sizeof(*ctx));
    ctx->ops = ops;
    ctx->threads = threads;
    ctx->vbat_threshold_uv = KPOC_VBAT_THRESHOLD_UV;

    atomic_init(&ctx->waiting_for_source, 0);
    atomic_init(&ctx->in_draw, 0);
    atomic_init(&ctx->power_key_down, 0);
    atomic_init(&ctx->exit_after_boot, 0);
    atomic_init(&ctx->long_press_satisfied, 0);
    atomic_init(&ctx->low_batt_logo_requested, 0);
    atomic_init(&ctx->key_trigger_suspend, 0);
    atomic_init(&ctx->key_short_press, 0);
    atomic_init(&ctx->health_ready, 0);
    atomic_init(&ctx->light_ready, 0);

    if (threads->mutex_init(ctx->m_main) != 0) goto fail;
    stage = 1;
    if (threads->mutex_init(ctx->m_state) != 0) goto fail;
    stage = 2;
    if (threads->mutex_init(ctx->m_draw) != 0) goto fail;
    stage = 3;
    if (threads->cond_init(ctx->c_draw) != 0) goto fail;
    stage = 4;
    if (threads->mutex_init(ctx->m_key) != 0) goto fail;
    stage = 5;
    if (threads->cond_init(ctx->c_key) != 0) goto fail;
    stage = 6;
    return 0;

fail:                                                                   
    if (stage >= 5) threads->mutex_destroy(ctx->m_key);
    if (stage >= 4) threads->cond_destroy(ctx->c_draw);
    if (stage >= 3) threads->mutex_destroy(ctx->m_draw);
    if (stage >= 2) threads->mutex_destroy(ctx->m_state);
    if (stage >= 1) threads->mutex_destroy(ctx->m_main);
    return -1;
}

void kpoc_ctx_deinit(KpocCtx *ctx)
{
    if (ctx == NULL || ctx->threads == NULL)
        return;
                                                     
    ctx->threads->cond_destroy(ctx->c_key);
    ctx->threads->mutex_destroy(ctx->m_key);
    ctx->threads->cond_destroy(ctx->c_draw);
    ctx->threads->mutex_destroy(ctx->m_draw);
    ctx->threads->mutex_destroy(ctx->m_state);
    ctx->threads->mutex_destroy(ctx->m_main);
}
