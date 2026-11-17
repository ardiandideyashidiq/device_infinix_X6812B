/*
 * workers.c - the three long-running worker bodies.
 *
 * The uevent worker waits on the input device for charger changes, the draw
 * worker pumps frames while the panel is on, and the key worker polls the
 * power key for short press and long press. All three cooperate through the
 * context mutexes; none of them owns the panel alone.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "kpoc.h"
#include "logo_render.h"
#include <string.h>

                                                                             
void kpoc_uevent_worker(KpocCtx *ctx)
{
    char buf[1024];
    int capacity;

    if (!ctx->uevent_inited) {
        if (ctx->ops->uevent_init() == 0) {
            kpoc_log(ctx, "uevent_init failed.\n");
            ctx->uevent_failed = 1;
            return;
        }
        ctx->uevent_inited = 1;
    }
    if (ctx->ops->uevent_next(buf, (size_t)(sizeof(buf) - 1)) < 1)
        return;

    kpoc_log(ctx, "on_uevent, %s\n", buf);
    if (strcasestr(buf, "power_supply/batt") == NULL)
        return;

                                                                         
                                                    
    if (!atomic_load(&ctx->health_ready)) {
        kpoc_log(ctx, "No health service, cannot get status\n");
        return;
    }
    if (!kpoc_health_source_available(ctx))
        kpoc_exit(ctx, KPOC_EXIT_SOURCE_LOST);

    capacity = kpoc_health_capacity(ctx);
    ctx->threads->mutex_lock(ctx->m_state);
    if (capacity < KPOC_FULL_CAPACITY) {
        ctx->state = KPOC_CHG_CHARGING;
        ctx->threads->mutex_unlock(ctx->m_state);
        kpoc_lights_chgon(ctx);                                              
    } else {
        ctx->state = KPOC_CHG_FULL;
        ctx->threads->mutex_unlock(ctx->m_state);
        kpoc_lights_chgfull(ctx);
    }
}

                                                                             
void kpoc_draw_worker(KpocCtx *ctx)
{
    uint64_t t, start_gen;
    int capacity, frame = 1, found = 0;
    int led_started, fb_unblanked;

    if (atomic_load(&ctx->waiting_for_source)) {
        t = ctx->ops->now_ms();
        while (!kpoc_time_exceed(ctx, t, 3000)) {
            if (kpoc_health_source_available(ctx)) {
                kpoc_log(ctx, "wait until charging source available\n");
                found = 1;
                break;
            }
            ctx->ops->sleep_ms(200);
        }
        if (!found)
            kpoc_log(ctx, "charging source not available for %d ms at KPOC starup\n", 3000);
        atomic_store(&ctx->waiting_for_source, 0);
    }

    kpoc_log(ctx, "draw thread working2...\n");
    atomic_store(&ctx->in_draw, 1);
    capacity = kpoc_health_capacity(ctx);                                  
    t = ctx->ops->now_ms();
    led_started = 0;
    fb_unblanked = 0;
    ctx->threads->mutex_lock(ctx->m_draw);
    start_gen = ctx->draw_wake_gen;
    ctx->threads->mutex_unlock(ctx->m_draw);
    while (!kpoc_time_exceed(ctx, t, KPOC_ANIM_BURST_MS) &&
           !atomic_load(&ctx->key_trigger_suspend)) {
        if (!kpoc_health_source_available(ctx))
            break;

        if (!fb_unblanked) {
            int fd = ctx->ops->fb_open(KPOC_PATH_FB0);
            if (fd < 0) {
                kpoc_log(ctx, "Failed to open fb0 device\n");
            } else {
                int rc = ctx->ops->fb_blank(fd, KPOC_FB_BLANK_UNBLANK);
                if (rc != 0)
                    kpoc_log(ctx, "Failed to unblank fb0 device\n");
                ctx->ops->fb_close(fd);
                fb_unblanked = 1;
            }
        }
        kpoc_bootlogo_show_charging(ctx, capacity, frame);
        if (logo_last_present_rc != 0)
            kpoc_log(ctx, "logo present idx=%u rc=%d\n",
                     logo_last_present_idx, logo_last_present_rc);
        if (!led_started) {
            ctx->ops->sleep_us(1000);
            kpoc_start_backlight(ctx);
            led_started = 1;
        }
        ctx->ops->sleep_ms(KPOC_ANIM_FRAME_MS);
        ++frame;
    }

                                                                           
    ctx->ops->light_set(KPOC_LIGHT_TYPE_BACKLIGHT, 0);
    kpoc_bootlogo_show_charging(ctx, capacity, 1);
    {
        int fd = ctx->ops->fb_open(KPOC_PATH_FB0);
        if (fd >= 0) {
            ctx->ops->fb_blank(fd, KPOC_FB_BLANK_POWERDOWN);
            ctx->ops->fb_close(fd);
        }
    }

                                                                        
                                                                          
                                                                        
                                                                          
               
    ctx->threads->mutex_lock(ctx->m_draw);
    atomic_store(&ctx->in_draw, 0);
    while (ctx->draw_wake_gen == start_gen)
        ctx->threads->cond_wait(ctx->c_draw, ctx->m_draw);
    ctx->threads->mutex_unlock(ctx->m_draw);
}

                                                                              
void kpoc_key_worker(KpocCtx *ctx)
{
    uint64_t t;
    int64_t voltage;
    int saw_press;

                                                                            
                                                                          
                                                                       
    ctx->threads->mutex_lock(ctx->m_key);
    if (ctx->key_wake_gen == ctx->key_last_gen) {
        ctx->ops->autosuspend_enable();
        ctx->threads->cond_wait(ctx->c_key, ctx->m_key);
        ctx->ops->autosuspend_disable();
    }
    ctx->key_last_gen = ctx->key_wake_gen;
    ctx->threads->mutex_unlock(ctx->m_key);

    saw_press = atomic_load(&ctx->power_key_down);
    kpoc_log(ctx, "pwr key long press check start\n");
    t = ctx->ops->now_ms();
    if (atomic_load(&ctx->power_key_down)) {
        while (atomic_load(&ctx->power_key_down) &&
               !kpoc_time_exceed(ctx, t, KPOC_LONG_PRESS_MS))
            ctx->ops->sleep_us(KPOC_KEY_POLL_US);
    }
    atomic_store(&ctx->long_press_satisfied,
                 atomic_load(&ctx->power_key_down) &&
                 kpoc_time_exceed(ctx, t, KPOC_LONG_PRESS_MS));

    if (atomic_load(&ctx->long_press_satisfied)) {
        kpoc_log(ctx, "pwr key reaches boot condition\n");
        voltage = kpoc_health_voltage_uv(ctx);
        if (voltage <= (int64_t)ctx->vbat_threshold_uv) {
            atomic_store(&ctx->low_batt_logo_requested, 1);
            kpoc_log(ctx, "VBAT <= %d\n", (int)ctx->vbat_threshold_uv);
        } else {
            atomic_store(&ctx->low_batt_logo_requested, 0);
            ctx->ops->input_watch_remove(KPOC_PATH_INPUT);
            kpoc_exit(ctx, KPOC_EXIT_BOOT_ANDROID);
        }
    } else if (saw_press) {
                                                                           
                                                                       
        atomic_store(&ctx->low_batt_logo_requested, 0);
    }

    kpoc_start_charging_anim(ctx, KPOC_ANIM_RESTART_REASON);
    kpoc_log(ctx, "pwr key long press check end\n");
}
