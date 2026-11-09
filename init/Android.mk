#
# Copyright (C) 2025 LineageOS
#
# SPDX-License-Identifier: Apache-2.0
#
# Two files have to sit outside /vendor or /system/etc:
#
#   fstab   - first stage init looks for /first_stage_ramdisk/fstab.${ro.hardware}
#             when the boot ramdisk is the recovery ramdisk
#             (BOARD_USES_RECOVERY_AS_BOOT), and for the recovery image it moves
#             to ${TARGET_RAMDISK_OUT} instead.
#   rc      - the recovery init.rc does `import /init.recovery.${ro.hardware}.rc`
#             at the root of the ramdisk, so a prebuilt_etc with recovery: true
#             (which lands in /system/etc) is never read.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE       := fstab.mt6768.ramdisk
LOCAL_MODULE_STEM  := fstab.mt6768
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := etc/fstab.mt6768
ifeq ($(BOARD_USES_RECOVERY_AS_BOOT),true)
LOCAL_MODULE_PATH  := $(TARGET_RECOVERY_ROOT_OUT)/first_stage_ramdisk
else
LOCAL_MODULE_PATH  := $(TARGET_RAMDISK_OUT)
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := init.recovery.mt6768.root.rc
LOCAL_MODULE_STEM  := init.recovery.mt6768.rc
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := etc/init.recovery.mt6768.rc
LOCAL_MODULE_PATH  := $(TARGET_RECOVERY_ROOT_OUT)
include $(BUILD_PREBUILT)
