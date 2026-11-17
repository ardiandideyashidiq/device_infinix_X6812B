/*
 * util.c - small helpers shared by the core.
 *
 * sysfs readers, boot-mode tag parsing, timeout comparisons and reboot
 * command construction. Kept free of platform state so the core can be
 * built off-device.
 */

#include "kpoc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void kpoc_log(KpocCtx *ctx, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    memcpy(buf, KPOC_LOG_PREFIX, sizeof(KPOC_LOG_PREFIX) - 1);
    va_start(ap, fmt);
    vsnprintf(buf + sizeof(KPOC_LOG_PREFIX) - 1,
              sizeof(buf) - (sizeof(KPOC_LOG_PREFIX) - 1), fmt, ap);
    va_end(ap);
    ctx->ops->log_str(KPOC_LOG_PRIO, buf);
}

                                                                                              
int kpoc_read_int_file(KpocCtx *ctx, const char *path)
{
    int value = 0;

    if (!path || ctx->ops->read_int_file(path, &value) != 0)
        return KPOC_READ_SENTINEL;
    return value;
}

                                                                                   
                                                                            
int kpoc_read_boot_mode(KpocCtx *ctx, const char *path,
                        uint32_t *size, uint32_t *tag, uint32_t *mode)
{
    if (!path || !size || !tag || !mode)
        return -1;
    return ctx->ops->read_boot_mode(path, size, tag, mode);
}

int kpoc_time_exceed(KpocCtx *ctx, uint64_t ref_ms, int timeout_ms)
{
    uint64_t now = ctx->ops->now_ms();

                                                          
    return now > ref_ms && (now - ref_ms) > (uint64_t)timeout_ms;
}

uint32_t kpoc_reboot_command(int reason)
{
    static const uint32_t table[6] = {
        KPOC_REBOOT_RESTART,                        
        KPOC_REBOOT_RESTART,                   
        KPOC_REBOOT_POWEROFF,                
        KPOC_REBOOT_POWEROFF,                  
        KPOC_REBOOT_POWEROFF,                  
        KPOC_REBOOT_RESTART                    
    };

    if (reason >= 0 && reason < 6)
        return table[reason];
    return KPOC_REBOOT_POWEROFF;              
}
