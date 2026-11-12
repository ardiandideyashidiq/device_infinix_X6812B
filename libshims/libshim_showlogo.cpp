/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayMode.h>
#include <ui/Size.h>
#include <utils/Timers.h>

#include <new>

namespace android {

struct DisplayConfig {
    ui::Size resolution;
    float xDpi = 0;
    float yDpi = 0;
    float refreshRate = 0;
    nsecs_t appVsyncOffset = 0;
    nsecs_t sfVsyncOffset = 0;
    nsecs_t presentationDeadline = 0;
    int configGroup = -1;
};

}  // namespace android

using namespace android;

extern "C" {

ui::Size _ZN7android2ui4Size7INVALIDE;

void _ZN7android21SurfaceComposerClient23getInternalDisplayTokenEv(sp<IBinder>* out) {
    const auto ids = SurfaceComposerClient::getPhysicalDisplayIds();
    new (out)
        sp<IBinder>(ids.empty() ? nullptr : SurfaceComposerClient::getPhysicalDisplayToken(ids[0]));
}

status_t
_ZN7android21SurfaceComposerClient22getActiveDisplayConfigERKNS_2spINS_7IBinderEEEPNS_13DisplayConfigE(
    const sp<IBinder>& display, DisplayConfig* config) {
    ui::DisplayMode mode;
    status_t res = SurfaceComposerClient::getActiveDisplayMode(display, &mode);
    if (res == NO_ERROR && config != nullptr) {
        config->resolution = mode.resolution;
        config->xDpi = mode.xDpi;
        config->yDpi = mode.yDpi;
        config->refreshRate = mode.vsyncRate;
        config->appVsyncOffset = mode.appVsyncOffset;
        config->sfVsyncOffset = mode.sfVsyncOffset;
        config->presentationDeadline = mode.presentationDeadline;
        config->configGroup = mode.group;
    }
    return res;
}

status_t _ZN7android21SurfaceComposerClient11Transaction5applyEb(
    SurfaceComposerClient::Transaction* thisptr, bool synchronous) {
    return thisptr->apply(synchronous, false);
}

void _ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataEPj(
    sp<SurfaceControl>* out, SurfaceComposerClient* thisptr, const String8& name, uint32_t w,
    uint32_t h, PixelFormat format, uint32_t flags, SurfaceControl* parent,
    gui::LayerMetadata metadata, uint32_t* outTransformHint) {
    new (out) sp<SurfaceControl>(
        thisptr->createSurface(name, w, h, format, static_cast<int32_t>(flags),
                               parent ? parent->getHandle() : nullptr, metadata, outTransformHint));
}

void _ZNK7android14SurfaceControl10getSurfaceEv(sp<Surface>* out, SurfaceControl* thisptr) {
    new (out) sp<Surface>(thisptr->getSurface());
}

}  // extern "C"
