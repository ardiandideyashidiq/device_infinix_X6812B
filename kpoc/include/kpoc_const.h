  
                                                                    
  
                                                                             
                                                                             
                                                                              
                        
   
#ifndef KPOC_CONST_H
#define KPOC_CONST_H

#include <stdint.h>
#include <limits.h>

                                                                             
#define KPOC_LOG_PRIO           3
#define KPOC_LOG_PREFIX         "<3>charger: "

                                                                             
#define KPOC_NICE               (-20)
#define KPOC_OOM_SCORE_ADJ      "-17"
#define KPOC_OOM_PATH           "/proc/self/oom_score_adj"

                                                                             
#define KPOC_REBOOT_RESTART     0x01234567u
#define KPOC_REBOOT_POWEROFF    0x4321FEDCu

                                                        
#define KPOC_EXIT_BOOT_ANDROID  0                                          
#define KPOC_EXIT_ALARM_BOOT    0                                           
#define KPOC_EXIT_FATAL         2                                         
#define KPOC_EXIT_ERROR_SHUTDOWN 4                                    
#define KPOC_EXIT_SOURCE_LOST   8                                                         

                                                                             
#define KPOC_PATH_BATTNOTIFY_0  "/sys/devices/platform/charger/BatteryNotify"
#define KPOC_PATH_BATTNOTIFY_1  "/sys/devices/platform/mt-battery/BatteryNotify"
#define KPOC_PATH_ADC_0         "/sys/devices/platform/charger/ADC_Charger_Voltage"
#define KPOC_PATH_ADC_1         "/sys/devices/platform/battery/ADC_Charger_Voltage"
#define KPOC_PATH_BOOTMODE      "/proc/device-tree/chosen/atag,boot"
#define KPOC_PATH_INPUT         "/dev/input"
#define KPOC_PATH_FB0           "/dev/graphics/fb0"
#define KPOC_PATH_LOGFILE       "/metadata/kpoc.log"
#define KPOC_PATH_RTC0          "/dev/rtc0"
#define KPOC_PATH_ALARM         "/dev/alarm"

                                                                              
#define KPOC_PATH_AC_ONLINE     "/sys/class/power_supply/ac/online"
#define KPOC_PATH_USB_ONLINE    "/sys/class/power_supply/usb/online"
#define KPOC_PATH_CHG_ONLINE    "/sys/class/power_supply/charger/online"
#define KPOC_PATH_BATT_CAPACITY "/sys/class/power_supply/battery/capacity"
#define KPOC_PATH_BATT_VOLTAGE  "/sys/class/power_supply/battery/voltage_now"

                                                                    
#define KPOC_PATH_LED_BLUE      "/sys/class/leds/blue/brightness"
#define KPOC_PATH_LCD_BACKLIGHT "/sys/class/leds/lcd-backlight/brightness"

                                                                             
#define KPOC_ADC_SOURCE_MV      2500                                                   
#define KPOC_FULL_CAPACITY      100
#define KPOC_VBAT_THRESHOLD_UV  3450000u                                                        
#define KPOC_BOOT_MODE_KPOC     9
#define KPOC_BACKLIGHT_LEVEL    20
#define KPOC_BACKLIGHT_COLOR    0xFF141414u                                          
#define KPOC_BLUE_LED_COLOR     0xFF0000FFu

                                                                             
#define KPOC_HEALTH_SERVICE     "default"
#define KPOC_HEALTH_BACKUP      "backup"
#define KPOC_HEALTH_IFACE       "android.hardware.health@2.0::IHealth"
#define KPOC_HEALTH_INFO_SIZE   0x70
#define KPOC_ADC_RETRY_MS       100

                                                  
#define KPOC_HI_AC              0x00
#define KPOC_HI_USB             0x01
#define KPOC_HI_WIRELESS        0x02
#define KPOC_HI_VOLTAGE_MV      0x1c

                                                                             
#define KPOC_LIGHT_SERVICE      "default"
#define KPOC_LIGHT_TYPE_BACKLIGHT   0
#define KPOC_LIGHT_TYPE_LED         3
#define KPOC_LIGHT_ID_INVALID       (-1)

                                                                             
                                                                   
                                                                        
                                                                            
                          
#define KPOC_ANIM_OLD           0
#define KPOC_ANIM_WIRED         1
#define KPOC_ANIM_WIRELESS      2
#define KPOC_ANIM_FRAME_MS      200
#define KPOC_ANIM_BURST_MS      6000
#define KPOC_ANIM_RESTART_REASON 8

                                                                             
#define KPOC_KEYCODE_POWER      116
#define KPOC_KEYCODE_ALT        107
#define KPOC_LONG_PRESS_MS      500
#define KPOC_KEY_POLL_US        100000
#define KPOC_INOTIFY_MASK       0x300u                                 
#define KPOC_EPOLL_MASK         0x20000001u
#define KPOC_EPOLL_MAX_EVENTS   16
#define KPOC_INPUT_EVENT_SIZE   16
#define KPOC_INPUT_STOP         INT_MIN                                      

                    
#define KPOC_EVIOCGNAME         0x804f4506u                      
#define KPOC_INPUT_NAME_MATCH_0 "kpd"
#define KPOC_INPUT_NAME_MATCH_1 "keypad"
#define KPOC_INPUT_NAME_MATCH_2 "pmic-keys"

                                                                             
#define KPOC_FBIOBLANK          0x4611u
#define KPOC_FB_BLANK_UNBLANK   0
#define KPOC_FB_BLANK_POWERDOWN 4

                                                                             
#define KPOC_READ_BUF_SIZE      32
#define KPOC_READ_SENTINEL      (-1)

                                                                             
#define KPOC_DBG_MUTEX_MAIN     0x1aab8u
#define KPOC_DBG_MUTEX_KEY      0x1aadcu
#define KPOC_DBG_COND_KEY       0x1aad8u
#define KPOC_DBG_MUTEX_DRAW     0x1aaf8u
#define KPOC_DBG_COND_DRAW      0x1ab00u
#define KPOC_DBG_MUTEX_STATE    0x1aafcu

#endif                   
