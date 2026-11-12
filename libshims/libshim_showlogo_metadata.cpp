/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gui/LayerMetadata.h>

namespace android {

struct LayerMetadata : public gui::LayerMetadata {
    LayerMetadata();
    status_t writeToParcel(Parcel* parcel) const override;
    status_t readFromParcel(const Parcel* parcel) override;
};

LayerMetadata::LayerMetadata() = default;

status_t LayerMetadata::writeToParcel(Parcel* parcel) const {
    return gui::LayerMetadata::writeToParcel(parcel);
}

status_t LayerMetadata::readFromParcel(const Parcel* parcel) {
    return gui::LayerMetadata::readFromParcel(parcel);
}

}  // namespace android
