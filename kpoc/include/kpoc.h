/*
 * kpoc.h - shared state and the public entry points of the charger daemon.
 *
 * Everything the daemon does is driven from a single KpocCtx: the uevent
 * watcher, the frame pump, the power-key handler and the alarm worker all
 * read and mutate it, so the mutexes in kpoc_const.h guard it.
 */

#ifndef KPOC_H
#define KPOC_H

#include <stdint.h>
#include <stddef.h>
#include "kpoc_const.h"
#include "kpoc_ops.h"

#ifndef __cplusplus
#  include <stdatomic.h>
   typedef atomic_int kpoc_atomic_int;
#else
                                                                                 
   typedef int kpoc_atomic_int;
#endif

#ifdef __cplusplus
extern "C" {
#endif

  
                           
                           
                                                                             
                                                                
                                                                             
                                                                       
                                                                          
                                         
                                                                         
                                                                           
                                                         
   

typedef enum {
    KPOC_CHG_IDLE      = 0,
    KPOC_CHG_FULL      = 1,
    KPOC_CHG_CHARGING  = 2,
    KPOC_CHG_NO_SOURCE = 3
} KpocChargerState;

                                                                            
typedef struct KpocHealthInfo {
    uint8_t  ac_online;
    uint8_t  usb_online;
    uint8_t  wireless_online;
    int32_t  capacity;
    int32_t  voltage_mv;
    int32_t  battery_status;
    int32_t  charger_type;
} KpocHealthInfo;

typedef struct KpocInputEvent {
    uint32_t type;
    uint32_t code;
    int32_t  value;
} KpocInputEvent;

typedef struct KpocCtx {
    const KpocOps       *ops;                      
    const KpocThreadOps *threads;                  
    KpocChargerState     state;                                

                                                                     
    kpoc_atomic_int waiting_for_source;
    kpoc_atomic_int in_draw;
    kpoc_atomic_int power_key_down;
    kpoc_atomic_int exit_after_boot;
    kpoc_atomic_int long_press_satisfied;
    kpoc_atomic_int low_batt_logo_requested;
    kpoc_atomic_int key_trigger_suspend;
    kpoc_atomic_int key_short_press;
    kpoc_atomic_int health_ready;
    kpoc_atomic_int light_ready;

    uint32_t backlight_level;
    uint32_t vbat_threshold_uv;

                                                                    
                                                                 
    int led_state;
    int blue_latched;
    int chg_on_type;

                                                                       
    uint64_t draw_wake_gen;
    uint64_t key_wake_gen;
    uint64_t key_last_gen;                                                    
    int uevent_inited;                                          
    int uevent_failed;

                                                                   
    uint64_t m_main[8];
    uint64_t m_state[8];
    uint64_t m_draw[8];
    uint64_t c_draw[8];
    uint64_t m_key[8];
    uint64_t c_key[8];
} KpocCtx;

                                
int  kpoc_ctx_init(KpocCtx *ctx, const KpocOps *ops, const KpocThreadOps *threads);
void kpoc_ctx_deinit(KpocCtx *ctx);

                                         
void kpoc_main_sequence(KpocCtx *ctx);
void kpoc_exit(KpocCtx *ctx, int reason) KPOC_NORETURN;
void kpoc_charging_control(KpocCtx *ctx);
void kpoc_key_control(KpocCtx *ctx);
void kpoc_long_press_control(KpocCtx *ctx);
void kpoc_alarm_control(KpocCtx *ctx);
void kpoc_alarm_worker(KpocCtx *ctx);

                             
int  kpoc_health_init(KpocCtx *ctx);
int  kpoc_health_get_info(KpocCtx *ctx, const char *who, KpocHealthInfo *out);
int  kpoc_health_source_available(KpocCtx *ctx);
int  kpoc_health_voltage_uv(KpocCtx *ctx);
int  kpoc_health_capacity(KpocCtx *ctx);
int  kpoc_battnotify_status(KpocCtx *ctx);
int  kpoc_is_wireless_charging(KpocCtx *ctx);
void kpoc_start_backlight(KpocCtx *ctx);

                             
int  kpoc_lights_init(KpocCtx *ctx);
void kpoc_set_light_brightness(KpocCtx *ctx, uint32_t type, uint32_t argb);
void kpoc_lights_chgon(KpocCtx *ctx);
void kpoc_lights_chgfull(KpocCtx *ctx);
void kpoc_lights_chgexit(KpocCtx *ctx);
void kpoc_start_blue_led(KpocCtx *ctx);

                                
void kpoc_set_draw_anim_mode(KpocCtx *ctx, int mode);
void kpoc_bootlogo_init(KpocCtx *ctx);
void kpoc_bootlogo_show_charging(KpocCtx *ctx, int capacity, int frame);
void kpoc_trigger_anim(KpocCtx *ctx);
void kpoc_start_charging_anim(KpocCtx *ctx, int reason);

                                                                         
void kpoc_uevent_worker(KpocCtx *ctx);
void kpoc_draw_worker(KpocCtx *ctx);
void kpoc_key_worker(KpocCtx *ctx);

                         
int  kpoc_read_int_file(KpocCtx *ctx, const char *path);
int  kpoc_read_boot_mode(KpocCtx *ctx, const char *path,
                         uint32_t *size, uint32_t *tag, uint32_t *mode);
int  kpoc_time_exceed(KpocCtx *ctx, uint64_t ref_ms, int timeout_ms);
uint32_t kpoc_reboot_command(int reason);
void kpoc_log(KpocCtx *ctx, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif             
