#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
from extract_utils.file import File
from extract_utils.fixups_blob import (
    BlobFixupCtx,
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)
from extract_utils.tools import (
    llvm_objdump_path,
)
from extract_utils.utils import (
    run_cmd,
)

namespace_imports = [
    'device/infinix/X6812B',
    'hardware/mediatek',
    'hardware/mediatek/libaedv',
    'hardware/mediatek/libmtkperf_client',
]


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'vendor.mediatek.hardware.videotelephony@1.0',
    ): lib_fixup_vendor_suffix,
}

blob_fixups: blob_fixups_user_type = {
    'vendor/lib/hw/audio.primary.mt6768.so': blob_fixup()
        .add_needed('libshim_audio.so')
        .replace_needed('libalsautils.so', 'libalsautils-mtk.so'),
    'vendor/etc/init/android.hardware.bluetooth@1.0-service-mediatek.rc': blob_fixup()
        .regex_replace(r'on property:vts[\s\S]*', ''),
    'vendor/lib64/hw/android.hardware.camera.provider@2.6-impl-mediatek.so': blob_fixup()
        .add_needed('libcamera_metadata_shim.so'),
    (
        'vendor/lib64/lib3a.flash.so',
    ): blob_fixup()
        .add_needed('liblog.so'),
    (
        'vendor/lib/libmtkcam_stdutils.so',
        'vendor/lib64/libmtkcam_stdutils.so',
        'vendor/lib/hw/dfps.mt6768.so',
        'vendor/lib64/hw/dfps.mt6768.so',
        'vendor/lib/hw/vendor.mediatek.hardware.pq@2.6-impl.so',
        'vendor/lib64/hw/vendor.mediatek.hardware.pq@2.6-impl.so',
    ): blob_fixup()
        .replace_needed('libutils.so', 'libutils-v30.so'),
    'vendor/lib64/libmtkcam_featurepolicy.so': blob_fixup()
        # evaluateCaptureConfiguration()
        .binary_regex_replace(b'\x34\xE8\x87\x40\xB9', b'\x34\x28\x02\x80\x52'),
    (
        'vendor/lib/libMtkOmxCore.so',
        'vendor/lib64/libMtkOmxCore.so',
    ): blob_fixup()
        .binary_regex_replace(b'mtk.vendor.omx.core.log', b'ro.vendor.mtk.omx.log\x00\x00'),
    'vendor/lib/libMtkOmxVdecEx.so': blob_fixup()
        .replace_needed('libui.so', 'libui-v32.so')
        .binary_regex_replace(b'ro.mtk_crossmount_support', b'ro.vendor.mtk_crossmount\x00')
        .binary_regex_replace(b'ro.mtk_deinterlace_support', b'ro.vendor.mtk_deinterlace\x00'),
    (
        'vendor/lib64/libwvhidl.so',
        'vendor/lib64/mediadrm/libwvdrmengine.so',
    ): blob_fixup()
        .replace_needed('libprotobuf-cpp-lite-3.9.1.so', 'libprotobuf-cpp-full-3.9.1.so')
        # Widevine engines were built against BoringSSL that still exported the
        # CBS_* accessors; current libcrypto keeps them hidden. The Lineage compat
        # shim puts CBS_init back (CBS_data/CBS_len too).
        .add_needed('libcrypto_shim.so'),
    (
        'vendor/bin/mnld',
        'vendor/lib/libaalservice.so',
        'vendor/lib64/libaalservice.so',
        'vendor/lib64/libcam.utils.sensorprovider.so',
    ): blob_fixup()
        .add_needed('android.hardware.sensors@1.0-convert-shared.so'),
    'vendor/lib/libmnl.so': blob_fixup()
        .add_needed('libcutils.so'),
    (
        'vendor/bin/hw/android.hardware.neuralnetworks@1.3-service-mtk-neuron',
        'vendor/lib/libnvram.so',
        'vendor/lib64/libnvram.so',
        'vendor/lib/libsysenv.so',
        'vendor/lib64/libsysenv.so',
    ): blob_fixup()
        .add_needed('libbase_shim.so'),
    'vendor/bin/hw/mtkfusionrild': blob_fixup()
        .add_needed('libutils-v32.so'),
    'vendor/lib64/hw/hwcomposer.mt6768.so': blob_fixup()
        .add_needed('libprocessgroup_shim.so'),
    (
        'vendor/lib/libutils-v30.so',
        'vendor/lib64/libutils-v30.so',
    ): blob_fixup()
        # ABI shim copied from the stock system image; it calls the plain-C
        # SetTaskProfiles/CgroupGetControllerPath that libprocessgroup no longer
        # exports, which hardware/lineage/compat libprocessgroup_shim provides
        .add_needed('libprocessgroup_shim.so'),
    'vendor/lib64/hw/sensors.mt6768.so': blob_fixup()
        .patchelf_version('0_17_2')
        .replace_needed('libutils.so', 'libutils-v30.so')
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so'),
    'vendor/lib64/libtranssion_bodybeauty.so': blob_fixup()
        .patchelf_version('0_17_2')
        .clear_symbol_version('AHardwareBuffer_allocate')
        .clear_symbol_version('AHardwareBuffer_describe')
        .clear_symbol_version('AHardwareBuffer_lockPlanes')
        .clear_symbol_version('AHardwareBuffer_release')
        .clear_symbol_version('AHardwareBuffer_unlock'),
}  # fmt: skip

module = ExtractUtilsModule(
    'X6812B',
    'infinix',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
