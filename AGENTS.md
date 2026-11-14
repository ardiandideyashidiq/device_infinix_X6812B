# AGENTS.md

Guidance for coding agents working on the Infinix X6812B (MT6768) LineageOS device tree.

## Project overview

- Target: LineageOS 22.2 (Android 15 QPR2, release `bp1a`), userdebug.
- SoC: MediaTek MT6768 (Helio G85 class), kernel 4.14.186, aarch64 + 32-bit.
- `$ANDROID_ROOT` is `../../..` from this tree (`/home/rd/lineage`).
- Stock dump: `/home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30`
- Tree history: forked from `realme_RMX2020`, sepolicy replaced from mt6781 X670 (`9529c41`).

Key paths:
- `device/infinix/X6812B` (this tree) | `device/infinix/X6812B-kernel` (prebuilt Image.gz, DTB, headers)
- `hardware/mediatek` (shared MTK HALs) | `device/mediatek/sepolicy_vndr` (shared MTK sepolicy)
- `vendor/infinix/X6812B` (generated — never edit)

## Hard rules (Non-negotiable)

1. **Never run a build.** Ask user to build and report `out/error.log`.
2. **Never propose "rebuild the kernel" as a fix.** Kernel source is unavailable; prebuilt only.
3. **Never run `grep`/`rg` at `$ANDROID_ROOT` or on `out/`.** Scope to specific subdirectories.
4. **Never edit AOSP/Lineage source or `vendor/infinix/X6812B`.** Fixes belong in this tree.
5. **Never `git reset --hard`.** Use `git checkout -- <path>` only on touched files.
6. **No explanatory comments in code/config files.** Put reasoning in commit messages.
7. **Check AOSP and MediaTek upstream before adding anything.** Avoid duplicate configs/props/services.
8. **Subsystem work requires reading playbooks.** Before editing audio, wifi/tethering, usb, telephony,
   battery, display, touch, sensors, or camera/media, agent MUST read `docs/playbooks/subsystems.md`.

## Debugging workflow (Order of attack)

1. **GitHub search first**: Search commit history & repos for the error (`search_commits` / `gh search commits '<error>'`). Most MTK/Lineage problems are already solved.
2. **Read AOSP / MediaTek source**: Trace the exact error message, struct, or service tag.
3. **Compare sibling trees**: Inspect `mt6768-dev/android_device_xiaomi_mt6768-common`.
4. **Reverse-engineer stock**: Check stock dump props, `.rc`, XMLs, and ELF blobs.
5. **Test live on device**: Verify on hardware (`adb push`, `setprop`, sysfs) before committing.

## Essential commands

```bash
# Regenerate vendor tree after ANY proprietary-files.txt change (must finish before building)
./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30

# Refresh pinned-blob sha1s (must leave `git diff` clean on second run)
python3 update-sha1sums.py
```

## Fast host validation (No build required)

Host binaries live in `out/host/linux-x86/bin/`. Detailed recipes: `docs/validation.md`.

```bash
# 1. SELinux policy & neverallow check
bash /tmp/opencode/run.sh # re-run m4 command for vendor_sepolicy.conf
out/host/linux-x86/bin/checkpolicy -C -M -c 30 -o /tmp/out.cil.raw \
  out/soong/.intermediates/system/sepolicy/vendor_sepolicy.conf/android_common/X6812B/vendor_sepolicy.conf

# 2. Context files check against precompiled policy
POLICY="../../../out/target/product/X6812B/vendor/etc/selinux/precompiled_sepolicy"
../../../out/host/linux-x86/bin/checkfc "$POLICY" sepolicy/vendor/file_contexts
../../../out/host/linux-x86/bin/checkfc -p "$POLICY" sepolicy/vendor/property_contexts

# 3. VINTF manifest & compatibility check
BOARD_SEPOLICY_VERS="202404" PRODUCT_ENFORCE_VINTF_MANIFEST=true \
  ../../../out/host/linux-x86/bin/assemble_vintf -i configs/manifests/manifest.xml \
  -o ../../../out/target/product/X6812B/vendor/etc/vintf/manifest.xml
../../../out/host/linux-x86/bin/checkvintf --check-one \
  --dirmap /vendor:../../../out/target/product/X6812B/vendor \
  --dirmap /apex:../../../out/target/product/X6812B/obj/PACKAGING/check_vintf_all_intermediates/apex \
  --property ro.boot.product.vendor.sku=

# 4. Init .rc syntax check
PASSWD="../../../system/core/libcutils/include/private/android_filesystem_config.h"
for rc in rootdir/etc/init/hw/*.rc; do
  [ -f "$rc" ] && ../../../out/host/linux-x86/bin/host_init_verifier -p "$PASSWD" "$rc"
done

# 5. Blueprint format
find . -name Android.bp -exec ../../../prebuilts/build-tools/linux-x86/bin/bpfmt -w {} +
```

## Documentation index

- `docs/validation.md`: Comprehensive host validation (sepolicy neverallows, check_elf_file, SONAME audit,
  aapt2 overlay compile, live-test RRO overlays, single ninja binary compile).
- `docs/triage-and-rules.md`: Build error triage, stage identification, known traps (SONAME, AIDL stubs,
  race conditions), SELinux rules, blob pinning rules, and dead-config audit.
- `docs/playbooks/subsystems.md`: Subsystem specifics for Audio, Wi-Fi/Tethering, USB/Ports,
  Telephony/RIL, Battery/Charging, Display/Refresh Rate, Touch/DT2W, Offline Charging, Sensors, UI
  Jank/Benchmarking/Offsets, and Camera/Codec2.

## Commit style

Structure: `<codename>: <subsystem>: <summary>` (e.g. `X6812B: wifi: enable WPA3-SAE softap support`).
1. Subject line: imperative, concise.
2. Log snippet: verbatim error, logcat trace, or failing command output showing the bug/repro.
3. Explanation: root cause, what was verified on hardware vs assumed, and why the chosen fix is correct.
No explanatory comments in code/config files; put all reasoning in the commit message.
