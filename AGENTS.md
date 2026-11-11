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