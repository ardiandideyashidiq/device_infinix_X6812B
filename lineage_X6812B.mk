#
# Copyright (C) 2018-2022 ArrowOS
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from device makefile
$(call inherit-product, device/infinix/X6812B/device.mk)

# Inherit some common crDroid stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Device identifier. This must come after all inclusions.
PRODUCT_NAME := lineage_X6812B
PRODUCT_DEVICE := X6812B
PRODUCT_BRAND := infinix
PRODUCT_MODEL := Infinix X6812B
PRODUCT_MANUFACTURER := infinix

PRODUCT_GMS_CLIENTID_BASE := android-transsion

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="sys_tssi_64_infinix-user 11 RP1A.200720.011 759119 release-keys" \
    BuildFingerprint=Infinix/X6812B-OP/Infinix-X6812B:11/RP1A.200720.011/241128V1218:user/release-keys \
    SystemName=$(PRODUCT_SYSTEM_NAME) \
    DeviceProduct=$(PRODUCT_SYSTEM_NAME)

# Time
LINEAGE_VERSION_APPEND_TIME_OF_DAY := true

$(call inherit-product-if-exists, vendor/gapps/arm64/arm64-vendor.mk)

WITH_GMS := true

PRODUCT_PRODUCT_PROPERTIES += persist.sys.ax_debug_enabled=1

AXION_CAMERA_REAR_INFO := 50
AXION_CAMERA_FRONT_INFO := 8

# Maintainer name (underscores become spaces in the UI)
AXION_MAINTAINER := R

# Processor name (underscores become spaces)
AXION_PROCESSOR := Mediatek Helio G88
