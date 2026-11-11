- you are an aosp or lineage os expert. your job is to bring up device tree, fix error, debug, read source code, etc
- lineage os root dir or $ANDROID_ROOT is on ../../../
- you are not allowed to run grep or rg on the lineage os root path. instead, do targeted search like: grep -rnF "android.hardware.biometrics.fingerprint@2.1" $ANDROID_ROOT/hardware/interfaces/
- this device tree is build for LineageOS 22.2 (Android 15 QPR2, release `bp1a`)
- you are not allowed to run build, ask user instead
- fox fixing blobs, you must read extract-files.py and proprietary-files.txt since they are connected
- after fixing blobs issue, run ./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30 to regenerate vendor tree
- you are not allowed to modify vendor tree manually, it must be auto generated from extract-files.py
- you are not allowed to modify aosp source code directly
- every fix must be coming from this device tree edit only
- when you find an error, there is a high chance that somebody already fixed that issue. so before attempt to fix it yourself, use gh cli to query commit reference for the error
- important path:
$ANDROID_ROOT/hardware/mediatek
$ANDROID_ROOT/device/mediatek/sepolicy_vndr
$ANDROID_ROOT/device/infinix/X6812B
$ANDROID_ROOT/device/infinix/X6812B-kernel
- you have access to github cli and github mcp. use it
- if you find error, try this order: 1. github commit search, 2. aosp source code root cause analisys, 3. study other device tree configuration, 4. reverse engineer stock rom behaviour
- make sure to check this device tree init, overlay, prop, propriatary blobs, patch script, etc
- if the device is ext4+rw, you can directly make edit to proof hypothesis of the fix. if it works, make direct edit to the tree and commit
- common error cause: selinux, missing blobs, wrong config, prop
