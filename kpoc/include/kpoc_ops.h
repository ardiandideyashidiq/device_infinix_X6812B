  
                                                                      
                                                                          
                                   
                             
   
#ifndef KPOC_OPS_H
#define KPOC_OPS_H

#include <stdint.h>
#include <stddef.h>

#if defined(__GNUC__)
#  define KPOC_NORETURN __attribute__((noreturn))
#else
#  define KPOC_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KpocHealthInfo KpocHealthInfo;                          
typedef struct KpocInputEvent KpocInputEvent;
typedef struct KpocCtx KpocCtx;

typedef enum {
    KPOC_WORKER_UEVENT = 0,
    KPOC_WORKER_DRAW   = 1,
    KPOC_WORKER_KEY    = 2,
    KPOC_WORKER_ALARM  = 3
} KpocWorkerKind;

  
                                                                          
                                       
                                                                             
                                                                   
                                                            
                                                                             
                                  
                                                                              
                                                                    
   
typedef struct KpocOps {
                                                                                    
    void (*log_str)(int prio, const char *msg);
    void (*reboot)(uint32_t cmd) KPOC_NORETURN;
    int  (*set_nice)(int nice);
    int  (*write_file)(const char *path, const char *text);

                           
    uint64_t (*now_ms)(void);
    void     (*sleep_ms)(unsigned ms);
    void     (*sleep_us)(unsigned us);

                             
    int  (*read_int_file)(const char *path, int *out);
    int  (*read_boot_mode)(const char *path, uint32_t *size,
                           uint32_t *tag, uint32_t *mode);

                                                                        
                                                                              
                                                                         
    int  (*health_init)(void);
    int  (*health_get_info)(KpocHealthInfo *out);
    int  (*health_get_capacity)(void);

                                                                             
                                                                     
    int  (*light_init)(void);
    int  (*light_set)(uint32_t type, uint32_t argb);

                     
    void (*anim_init)(void);
    void (*anim_set_version)(int version);
    void (*anim_set_draw_mode)(int mode);
    void (*anim_show_capacity)(uint32_t capacity);                             
    void (*anim_show_ov_logo)(void);
    void (*anim_show_low_battery)(void);

                          
    void (*autosuspend_enable)(void);
    void (*autosuspend_disable)(void);
    int  (*uevent_init)(void);
    int  (*uevent_next)(char *buf, size_t len);

                            
    int  (*input_scan_begin)(void);
    int  (*input_scan_next)(char *name, size_t len);
    void (*input_scan_end)(void);
    int  (*input_open)(const char *dev);
    int  (*input_next_event)(KpocInputEvent *ev);
    int  (*input_watch_add)(const char *dev);
    int  (*input_watch_remove)(const char *dev);
    int  (*input_dispatch)(void);                                              

                     
    int  (*fb_open)(const char *path);
    int  (*fb_blank)(int fd, int mode);
    void (*fb_close)(int fd);

                                                                             
                                                                           
    int  (*alarm_wait_power_on)(void);
} KpocOps;

                                                                            
typedef void *(*KpocThreadFn)(void *);

  
                                                                          
                                    
                                                                     
                                                                   
                                                                       
                                                                     
                                                                          
                                                                          
   
typedef struct KpocThreadOps {
    int  (*create)(KpocWorkerKind kind, KpocThreadFn fn, KpocCtx *arg);
    int  (*mutex_init)(void *mutex);
    int  (*mutex_destroy)(void *mutex);
    int  (*mutex_lock)(void *mutex);
    int  (*mutex_unlock)(void *mutex);
    int  (*cond_init)(void *cond);
    int  (*cond_destroy)(void *cond);
    int  (*cond_wait)(void *cond, void *mutex);                               
    int  (*cond_signal)(void *cond);
} KpocThreadOps;

                                                                                
const KpocOps       *kpoc_ops_real(void);
const KpocThreadOps *kpoc_threads_real(void);

#ifdef __cplusplus
}
#endif

#endif                 
