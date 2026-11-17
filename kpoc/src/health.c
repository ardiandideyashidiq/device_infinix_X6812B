  
                                                                  
                             
   
#include "kpoc.h"
#include <limits.h>

int kpoc_health_init(KpocCtx *ctx)
{
    int r = ctx->ops->health_init();

    if (r != 0) {
        kpoc_log(ctx, "Could not retrieve health service\n");
    } else {
        atomic_store(&ctx->health_ready, 1);
    }
    return r;
}

int kpoc_health_get_info(KpocCtx *ctx, const char *who, KpocHealthInfo *out)
{
    int r;

    if (!atomic_load(&ctx->health_ready)) {
        kpoc_log(ctx, "%s: No health service, cannot get status\n", who);
        kpoc_log(ctx, "%s: cannot get HealthInfo\n", who);
        return -1;
    }
    r = ctx->ops->health_get_info(out);
    if (r < 0) {
        kpoc_log(ctx, "transaction error\n");
        kpoc_log(ctx, "%s: cannot get HealthInfo\n", who);
    }
    return r;
}

  
                                                                         
                                                                             
   
int kpoc_health_source_available(KpocCtx *ctx)
{
    KpocHealthInfo info;
    int flags, vchr, present;

    if (kpoc_health_get_info(ctx, "is_charging_source_available", &info) < 0)
        return 0;

    flags = (info.usb_online ? 1 : 0) |
            (info.ac_online ? 1 : 0) |
            (info.wireless_online ? 1 : 0);

    vchr = kpoc_read_int_file(ctx, KPOC_PATH_ADC_0);
    if (vchr == KPOC_READ_SENTINEL)
        vchr = kpoc_read_int_file(ctx, KPOC_PATH_ADC_1);

    kpoc_log(ctx, "in %s(), usb:%d ac:%d wireless:%d vchr:%d\n",
             "is_charging_source_available", info.usb_online, info.ac_online,
             info.wireless_online, vchr);

    present = flags | (vchr >= KPOC_ADC_SOURCE_MV ? 1 : 0);
    return present != 0;
}

int kpoc_health_voltage_uv(KpocCtx *ctx)
{
    KpocHealthInfo info;
    int64_t uv;

    if (kpoc_health_get_info(ctx, "get_voltage", &info) < 0)
        return 0;
    kpoc_log(ctx, "get_voltage: batt_vol: %d\n", info.voltage_mv);
    uv = (int64_t)info.voltage_mv * 1000;
    if (uv > INT_MAX) uv = INT_MAX;
    if (uv < INT_MIN) uv = INT_MIN;
    return (int)uv;
}

  
                                                                        
                                                                            
                                                                        
   
int kpoc_health_capacity(KpocCtx *ctx)
{
    if (!atomic_load(&ctx->health_ready)) {
        kpoc_log(ctx, "get_health_capacity: No health service, cannot get status\n");
        return 0;                                                         
    }
    for (;;) {
        int value = ctx->ops->health_get_capacity();
        if (value >= 0) {
            kpoc_log(ctx, "get_capacity: bat_level: %d\n", value);
            return value;
        }
        if (value != KPOC_READ_SENTINEL) {
            kpoc_log(ctx, "get_capacity: cannot get capacity\n");
            return 0;                                                    
        }
        ctx->ops->sleep_ms(KPOC_ADC_RETRY_MS);                        
    }
}

                                                                               
int kpoc_battnotify_status(KpocCtx *ctx)
{
    static const char *const paths[2] = {
        KPOC_PATH_BATTNOTIFY_0, KPOC_PATH_BATTNOTIFY_1
    };
    int value = KPOC_READ_SENTINEL, idx = 2, i;

    for (i = 0; i < 2; ++i) {
        value = kpoc_read_int_file(ctx, paths[i]);
        if (value != KPOC_READ_SENTINEL) {
            idx = i;
            break;
        }
    }
    kpoc_log(ctx, "charger battStatus=%d, idx=%d\n", value, idx);
    return value > 0;
}

int kpoc_is_wireless_charging(KpocCtx *ctx)
{
    KpocHealthInfo info;

    if (kpoc_health_get_info(ctx, "is_wireless_charging", &info) < 0)
        return 0;                                                          
    kpoc_log(ctx, "wireless_charging: %d\n", info.wireless_online != 0);
    return info.wireless_online != 0;
}

                                                                       
                                                                       
void kpoc_start_backlight(KpocCtx *ctx)
{
    uint32_t size = 0, tag = 0, mode = 0;
    KpocHealthInfo info;
    uint32_t level = 150;
    int ac = 0;

                                                                            
                                                                  
    if (kpoc_read_boot_mode(ctx, KPOC_PATH_BOOTMODE, &size, &tag, &mode) < 0)
        kpoc_log(ctx, "Fail to open boot mode file  %s\n", KPOC_PATH_BOOTMODE);

    if (kpoc_health_get_info(ctx, "start_backlight", &info) < 0)
        return;                                               
    ac = info.ac_online;

    kpoc_log(ctx, "val_bootmode = %d, val_ac = %d\n", (int)mode, ac);
    if (mode == KPOC_BOOT_MODE_KPOC && ac == 0)
        level = KPOC_BACKLIGHT_LEVEL;

    ctx->backlight_level = level;
    kpoc_set_light_brightness(ctx, KPOC_LIGHT_TYPE_BACKLIGHT,
                              0xFF000000u | (level << 16) | (level << 8) | level);
}
