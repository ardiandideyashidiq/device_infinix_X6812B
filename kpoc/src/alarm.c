  
                                         
                                                                        
                                                                       
                                                                      
   
#include "kpoc.h"

void kpoc_alarm_worker(KpocCtx *ctx)
{
    int result;

    if (ctx->ops->alarm_wait_power_on == NULL)
        return;                                 

    result = ctx->ops->alarm_wait_power_on();
    if (result > 0) {
        kpoc_log(ctx, "rtc trigger by alarm\n");
        kpoc_exit(ctx, KPOC_EXIT_ALARM_BOOT);
        return;
    }
    if (result < 0)
        kpoc_exit(ctx, KPOC_EXIT_ERROR_SHUTDOWN);
}

static void *alarm_thread_fn(void *arg)
{
    KpocCtx *ctx = (KpocCtx *)arg;
    kpoc_log(ctx, "alarm thread start\n");
    for (;;)
        kpoc_alarm_worker(ctx);
    return NULL;
}

void kpoc_alarm_control(KpocCtx *ctx)
{
    if (ctx->ops->alarm_wait_power_on == NULL)
        return;
    if (ctx->threads->create(KPOC_WORKER_ALARM, alarm_thread_fn, ctx) != 0) {
        kpoc_log(ctx, "create alarm pthread failed.\n");
        kpoc_exit(ctx, KPOC_EXIT_ERROR_SHUTDOWN);
    }
}
