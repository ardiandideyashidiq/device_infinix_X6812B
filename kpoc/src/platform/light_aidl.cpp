  
                                                                              
                                                                              
                                                           
  
                                                                        
                                                                           
                                                                
   
#include "kpoc.h"

#ifdef KPOC_ANDROID
#include <aidl/android/hardware/light/HwLight.h>
#include <aidl/android/hardware/light/HwLightState.h>
#include <aidl/android/hardware/light/ILights.h>
#include <android/binder_manager.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <vector>

namespace light = ::aidl::android::hardware::light;

static std::shared_ptr<light::ILights> g_light;
static std::vector<light::HwLight> g_lights;

static int write_int_file(const char *path, int value)
{
    char buf[32];
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    ssize_t n;
    if (fd < 0)
        return -errno;
    n = snprintf(buf, sizeof(buf), "%d\n", value);
    if (write(fd, buf, (size_t)n) != n) {
        int saved = errno;
        close(fd);
        return -saved;
    }
    close(fd);
    return 0;
}

static int sysfs_set_light(uint32_t type, uint32_t argb)
{
    int level = (argb == 0) ? 0 : ((argb & 0xffu) != 0 ? (int)(argb & 0xffu) : 1);
    if (type == KPOC_LIGHT_TYPE_BACKLIGHT)
        return write_int_file(KPOC_PATH_LCD_BACKLIGHT, level);
    if (type == KPOC_LIGHT_TYPE_LED)
        return write_int_file(KPOC_PATH_LED_BLUE, (argb == 0) ? 0 : 255);
    return -22;
}

static void try_acquire_light(void)
{
    ndk::SpAIBinder binder(
        AServiceManager_checkService("android.hardware.light.ILights/default"));
    if (binder.get() == nullptr)
        return;
    g_light = light::ILights::fromBinder(binder);
    if (g_light != nullptr) {
        g_lights.clear();
        (void)g_light->getLights(&g_lights);
    }
}

extern "C" int kpoc_aidl_light_init(void)
{
    try_acquire_light();
    return 0;
}

extern "C" int kpoc_aidl_light_set(uint32_t type, uint32_t argb)
{
    if (g_light == nullptr)
        try_acquire_light();
    if (g_light == nullptr)
        return sysfs_set_light(type, argb);

    int32_t id = KPOC_LIGHT_ID_INVALID;
    for (const auto &l : g_lights) {
        if (static_cast<uint32_t>(l.type) == type)
            id = l.id;
    }

    light::HwLightState st;
    st.color = static_cast<int32_t>(argb);
    st.flashMode = light::FlashMode::NONE;
    st.flashOnMs = 0;
    st.flashOffMs = 0;
    st.brightnessMode = light::BrightnessMode::USER;

    auto status = g_light->setLightState(id, st);
    if (!status.isOk())
        return sysfs_set_light(type, argb);
    return 0;
}
#else                    
extern "C" int kpoc_aidl_light_init(void) { return -1; }
extern "C" int kpoc_aidl_light_set(uint32_t, uint32_t) { return -1; }
#endif
