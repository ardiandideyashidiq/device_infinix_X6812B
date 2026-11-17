/*
 * lights.c - backlight and charging LED.
 *
 * Talks to the AIDL light HAL, falling back to the sysfs nodes when the HAL
 * is not registered yet. At charger start the HAL is usually not up: both
 * are queued from on charger with undefined file order, so the binder is
 * re-acquired on every call rather than cached at init.
 */

#include "kpoc.h"

int kpoc_lights_init(KpocCtx *ctx)
{
    int r = ctx->ops->light_init();

    if (r != 0) {
        kpoc_log(ctx, "Error getting HAL for Ilights\n");
    } else {
        atomic_store(&ctx->light_ready, 1);
    }
    return r;
}

void kpoc_set_light_brightness(KpocCtx *ctx, uint32_t type, uint32_t argb)
{
    int r;

    if (!atomic_load(&ctx->light_ready)) {
        kpoc_log(ctx, "No light service, cannot set brightness\n");
        return;
    }
    r = ctx->ops->light_set(type, argb);
    if (r < 0)
        kpoc_log(ctx, "Failed to set light to 0x%x for type %d, ret=%d\n", argb, type, r);
    else
        kpoc_log(ctx, "set light to 0x%x for type %d successfully\n", argb, type);
}

                                                                             
static void blue_locked(KpocCtx *ctx)
{
    if (ctx->led_state != 1) {
        kpoc_set_light_brightness(ctx, KPOC_LIGHT_TYPE_LED, KPOC_BLUE_LED_COLOR);
        ctx->led_state = 1;
    }
}

void kpoc_start_blue_led(KpocCtx *ctx)
{
    ctx->threads->mutex_lock(ctx->m_main);
    blue_locked(ctx);
    ctx->threads->mutex_unlock(ctx->m_main);
}

                                                                              
void kpoc_lights_chgon(KpocCtx *ctx)
{
    kpoc_log(ctx, "lights_chgon");
    ctx->threads->mutex_lock(ctx->m_main);
    if (ctx->chg_on_type != 2) {
        kpoc_set_light_brightness(ctx, KPOC_LIGHT_TYPE_LED, 0);
        ctx->chg_on_type = 2;
    }
    blue_locked(ctx);
    ctx->threads->mutex_unlock(ctx->m_main);
}

                                                                          
void kpoc_lights_chgfull(KpocCtx *ctx)
{
    kpoc_log(ctx, "lights_chgfull");
    ctx->threads->mutex_lock(ctx->m_main);
    if (!ctx->blue_latched) {
        blue_locked(ctx);
        ctx->blue_latched = 1;
    }
    if (ctx->led_state != 2) {
        kpoc_set_light_brightness(ctx, KPOC_LIGHT_TYPE_LED, 0);
        ctx->led_state = 2;
    }
    ctx->threads->mutex_unlock(ctx->m_main);
}

                                                              
void kpoc_lights_chgexit(KpocCtx *ctx)
{
    kpoc_log(ctx, "lights_chgexit");
    ctx->threads->mutex_lock(ctx->m_main);
    if (ctx->led_state != 2) {
        kpoc_set_light_brightness(ctx, KPOC_LIGHT_TYPE_LED, 0);
        ctx->led_state = 2;
    }
    ctx->threads->mutex_unlock(ctx->m_main);
}
