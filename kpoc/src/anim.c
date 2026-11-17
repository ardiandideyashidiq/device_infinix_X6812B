  
                                                                   
                             
   
#include "kpoc.h"

void kpoc_set_draw_anim_mode(KpocCtx *ctx, int mode)
{
    ctx->ops->anim_set_draw_mode(mode);
    ctx->ops->log_str(7,
        "<7>charger: [ChargingAnimation set_draw_anim_mode 0]under kernel power off charging mode, use fb to draw animation!");
    ctx->ops->log_str(7,
        "<7>charger: [ChargingAnimation set_draw_anim_mode 0]kpoc flag == 1 ? : running_mode = 1 \n ");
}

void kpoc_bootlogo_init(KpocCtx *ctx)
{
    int version = kpoc_is_wireless_charging(ctx)
                      ? KPOC_ANIM_WIRELESS : KPOC_ANIM_WIRED;
    kpoc_log(ctx, "bootlogo_init\n");
    kpoc_log(ctx, "sync_anim_version\n");
    ctx->ops->anim_set_version(version);
    ctx->ops->anim_init();
}

  
                                                                          
                                                                               
                                                                
   
void kpoc_bootlogo_show_charging(KpocCtx *ctx, int capacity, int frame)
{
    int status = kpoc_battnotify_status(ctx);

    kpoc_log(ctx, "[ChargingAnimation: %s %d]\n", "bootlogo_show_charging", frame);
    if (status) {
        kpoc_log(ctx, "[ChargingAnimation: %s %d] show_charger_error_logo, get_battnotify_status()= %d \n",
                 "bootlogo_show_charging", frame, status);
        ctx->ops->anim_show_ov_logo();
        return;
    }
    if (atomic_load(&ctx->low_batt_logo_requested)) {
        kpoc_log(ctx, "[ChargingAnimation: %s %d] show_low_battery , showLowBattLogo = %d \n",
                 "bootlogo_show_charging", frame, 1);
        ctx->ops->anim_show_low_battery();
        return;
    }
    kpoc_log(ctx, "[ChargingAnimation: %s %d]%d, %d\n",
             "bootlogo_show_charging", frame, capacity, frame);
    ctx->ops->anim_show_capacity(capacity < 0 ? 0u : (uint32_t)capacity);
}

                                                                             
                                                               
void kpoc_trigger_anim(KpocCtx *ctx)
{
    ctx->threads->mutex_lock(ctx->m_draw);
    if (atomic_load(&ctx->in_draw)) {
        atomic_store(&ctx->key_trigger_suspend, 1);
        ctx->threads->mutex_unlock(ctx->m_draw);
        return;
    }
    if (kpoc_health_source_available(ctx)) {
        atomic_store(&ctx->key_trigger_suspend, 0);
        ++ctx->draw_wake_gen;
        ctx->threads->cond_signal(ctx->c_draw);
    } else {
        ctx->threads->mutex_unlock(ctx->m_draw);
        kpoc_log(ctx, "no charging source, skip drawing anim\n");
        return;
    }
    ctx->threads->mutex_unlock(ctx->m_draw);
}

void kpoc_start_charging_anim(KpocCtx *ctx, int reason)
{
    kpoc_log(ctx, "[ChargingAnimation: %s %d]\n", "start_charging_anim", reason);
    kpoc_trigger_anim(ctx);
}
