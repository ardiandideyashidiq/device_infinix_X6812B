- you are an aosp or lineage os expert. your job is to bring up device tree, fix error, debug, read source code, etc
- lineage os root dir or $ANDROID_ROOT is on ../../../
- you are not allowed to run grep or rg on the lineage os root path. instead, do targeted search like: grep -rnF "android.hardware.biometrics.fingerprint@2.1" $ANDROID_ROOT/hardware/interfaces/
- this device tree is build for LineageOS 22.2 (Android 15 QPR2, release `bp1a`)
- you are not allowed to run build, ask user instead
- for fixing blobs, you must read extract-files.py and proprietary-files.txt since they are connected
- after fixing blobs issue, run ./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30 to regenerate vendor tree
- you are not allowed to modify vendor tree manually, it must be auto generated from extract-files.py
- you are not allowed to modify aosp source code directly
- every fix must be coming from this device tree edit only
- when you find an error, there is a high chance that somebody already fixed that issue. so before attempt to fix it yourself, use gh cli to query commit reference for the error. example: gh search commits 'xyz'
- important path:
$ANDROID_ROOT/hardware/mediatek
$ANDROID_ROOT/device/mediatek/sepolicy_vndr
$ANDROID_ROOT/device/infinix/X6812B
$ANDROID_ROOT/device/infinix/X6812B-kernel
- you have access to github cli and github mcp. use it
- if you find error, try this order:
  1. github commit search (query commit reference with gh cli or github mcp)
  2. aosp source code root cause analysis (read exact framework/HAL code, structs, and log tags)
  3. study other device tree configuration (e.g. mt6768-dev/android_device_xiaomi_mt6768-common)
  4. reverse engineer stock rom behaviour (/home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30 props, init, media/audio xmls, and binaries)
  5. live device profiling and hypothesis testing (measure before/after on ext4+rw vendor overlay, check logs and dumpsys)
- make sure to check this device tree init, overlay, prop, proprietary blobs, patch script, etc
- for UI/jank debugging: check both 60Hz and 90Hz refresh rates, inspect dumpsys SurfaceFlinger (HWC/GPU missed frames, phase offsets), dumpsys gfxinfo <pkg> framestats (UI/RenderThread/GPU times), FrameTracker CUJ logs, and check process scheduling groups (e.g. top-app vs foreground)
- for power/performance debugging: check PowerHAL node fds and hint stats (dumpsys android.hardware.power.IPower/default), CPU/GPU clock scaling (/sys/devices/system/cpu/cpufreq/, /sys/kernel/ged/hal/), EAS/stune boosts (/dev/stune/), and watch for kernel ABI mismatches (e.g. 4.14 kernel rejecting newer uclamp sched_setattr calls with E2BIG/err=7)
- if the device is ext4+rw, you can directly make edit to proof hypothesis of the fix. if it works, make direct edit to the tree and commit. if it is erofs, dont force to remount
- common error cause: selinux, missing blobs, wrong config, prop, symlink, kernel ABI mismatch, scheduling group differences
- make sure to not make a duplicate, check if aosp already source built it. this affect .rc, selinux, and blobs
- dont make explanation comment on file, instead, do it in commit message
- for audio debugging:
  - never rely solely on `dumpsys media_session` or app-level `PlaybackState` to conclude audio is playing. Verify `dumpsys media.audio_flinger` (check `Standby: yes/no`, active tracks, written frames, and underruns).
  - logcat main/system buffers wrap quickly when verbose HALs (e.g. `powerhal-libperfmgr` sched_setattr) flood logs. Always inspect audio HAL PID and check `dmesg` or direct dumpsys before concluding logs are missing.
  - MediaTek stock audio uses DTS processing (`libdtsaudio.so`, `mtk_bessound`). In AOSP, device effects with `EFFECT_FLAG_TYPE_POST_PROC` can be rejected on stream output sessions unless patched or handled via software fallback.
- for touch gestures & double-tap to wake (DT2W):
  - inspect driver wake keys in kernel (`Image.gz` disassembly of `*_ts_work_func`). MTK panels (Novatek, Focaltech) often emit key code 183 (`KEY_F13`) or 143 (`KEY_WAKEUP`) to `/dev/input/event*` while suspended.
  - without a custom keylayout (`usr/keylayout/mtk-tpd.kl`), Android falls back to `Generic.kl` where key 183 maps to `F13` (not marked with `WAKE` flag in PhoneWindowManager), discarding the wake event.
  - verify PowerHAL `powerhint.json`: `DoubleTapToWakeEnable` node path `/proc/gesture_function` requires `"cc1"` to enable and `"cc2"` to disable (or check `/proc/gesture_function` directly).
- for display cutout & status bar padding overlays:
  - always inspect the physical punch-hole / notch position on the actual device: compare `FrameworksResTarget` `config_mainBuiltInDisplayCutout` and `status_bar_height_portrait` with stock RRO values.
  - in `SystemUIResTarget`, check `rounded_corner_content_padding`, `status_bar_padding_start`, and `status_bar_padding_top`. Excessive padding or missing status bar height overrides causes clock/icon horizontal clipping or asymmetric alignment.
- for offline charging (`kpoc_charger`):
  - MTK `kpoc_charger` is a 32-bit ARM executable (`system/bin/kpoc_charger`) requiring 32-bit `system/lib/libshowlogo.so`. Extract both in `proprietary-files.txt` and ensure `compile_multilib: "32"` in `libshims/Android.bp`.
  - `kpoc_charger` requires `libsuspend` (`system/core/libsuspend`) in `device.mk` (`PRODUCT_PACKAGES += libsuspend`).
  - `init.mt6768.rc` in `on charger` must start `vendor.light-default` (AIDL light HAL service name, NOT `vendor.light`) and chown LED brightness sysfs nodes.
  - remove any conflicting `service vendor.charger /system/bin/charger` in `init.mt6768.rc`: `kpoc_charger.rc` defines `service kpoc_charger` in `class charger`.
  - 32-bit Android 15 `libgui`/`libui` shimming pitfalls:
    1. returning `sp<T>` in `extern "C"` triggers `-Wreturn-type-c-linkage` under `-Werror`. Under ARM32 AAPCS, `sp<T>` is returned via sret pointer in `r0`; write placement `new (out) sp<T>(...)` taking `sp<T>* out` as first parameter.
    2. `SurfaceComposerClient.h` contains `using gui::LayerMetadata;` inside `namespace android`. Do not declare `struct LayerMetadata` in the same TU; split `android::LayerMetadata` ctor/vtable symbols into a separate TU (`libshim_showlogo_metadata.cpp`).
    3. in `getActiveDisplayConfig`, Android 15 `ui::DisplayMode` has an `id` field at `+0` and `std::vector` at `+20`. Never cast `DisplayConfig*` to `ui::DisplayMode*` directly (causes stack corruption and 4-byte offset shift); copy `resolution`, `dpi`, and `refreshRate` explicitly.
- you need to update this file constantly after you fix something. update how to debug an issue, how to fix it, what mistakes you have made, etc.
