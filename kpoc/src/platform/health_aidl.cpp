  
                                                                                
                                                                               
                                                           
  
                                                                         
                                                                          
                                                                             
                                                            
   
#include "kpoc.h"

#ifdef KPOC_ANDROID
#include <aidl/android/hardware/health/HealthInfo.h>
#include <aidl/android/hardware/health/IHealth.h>
#include <android/binder_manager.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace health = ::aidl::android::hardware::health;

static std::shared_ptr<health::IHealth> g_health;

static bool read_int_file(const char *path, int *out)
{
    char buf[KPOC_READ_BUF_SIZE + 1];
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    ssize_t n;
    if (fd < 0)
        return false;
    n = read(fd, buf, KPOC_READ_BUF_SIZE);
    close(fd);
    if (n < 1)
        return false;
    buf[n] = '\0';
    *out = atoi(buf);
    return true;
}

static int sysfs_get_info(KpocHealthInfo *out)
{
    int value = 0;

    out->ac_online = read_int_file(KPOC_PATH_AC_ONLINE, &value) && value > 0 ? 1 : 0;
    out->usb_online = read_int_file(KPOC_PATH_USB_ONLINE, &value) && value > 0 ? 1 : 0;
    out->wireless_online = 0;                                           

    if (read_int_file(KPOC_PATH_BATT_CAPACITY, &value))
        out->capacity = value;
                                                                    
    if (read_int_file(KPOC_PATH_BATT_VOLTAGE, &value))
        out->voltage_mv = value / 1000;
    if (read_int_file(KPOC_PATH_CHG_ONLINE, &value) && value > 0)
        out->ac_online = 1;

    return 0;
}

extern "C" int kpoc_aidl_health_init(void)
{
    ndk::SpAIBinder binder(
        AServiceManager_checkService("android.hardware.health.IHealth/default"));
    if (binder.get() == nullptr)
        binder = ndk::SpAIBinder(
            AServiceManager_checkService("android.hardware.health.IHealth/backup"));
    if (binder.get() != nullptr)
        g_health = health::IHealth::fromBinder(binder);

                                                                         
    return 0;
}

extern "C" int kpoc_aidl_health_get_info(void *out)
{
    KpocHealthInfo *info = static_cast<KpocHealthInfo *>(out);
    health::HealthInfo result;
    if (info == nullptr)
        return -22;              

    if (g_health == nullptr)
        return sysfs_get_info(info);

    auto status = g_health->getHealthInfo(&result);
    if (!status.isOk())
        return sysfs_get_info(info);

    info->ac_online = result.chargerAcOnline ? 1 : 0;
    info->usb_online = result.chargerUsbOnline ? 1 : 0;
    info->wireless_online = result.chargerWirelessOnline ? 1 : 0;
    info->capacity = result.batteryLevel;
    info->voltage_mv = result.batteryVoltageMillivolts;
    info->battery_status = static_cast<int32_t>(result.batteryStatus);
    info->charger_type = 0;
    return 0;
}

extern "C" int kpoc_aidl_health_get_capacity(void)
{
    int value = 0;
    if (g_health == nullptr) {
        if (!read_int_file(KPOC_PATH_BATT_CAPACITY, &value))
            return 0;                                                         
        return value;
    }
    auto status = g_health->getCapacity(&value);
    if (!status.isOk())
        return 0;
    return value;
}
#else                    
extern "C" int kpoc_aidl_health_init(void) { return -1; }
extern "C" int kpoc_aidl_health_get_info(void *) { return -22; }
extern "C" int kpoc_aidl_health_get_capacity(void) { return -1; }
#endif
