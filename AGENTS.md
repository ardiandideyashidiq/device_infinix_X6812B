# AGENTS.md

Guidance for coding agents working on the Infinix X6812B (MT6768) LineageOS
device tree.

## Project overview

- Target: LineageOS 22.2 (Android 15 QPR2, release `bp1a`), userdebug.
- SoC: MediaTek MT6768 (Helio G85 class), kernel 4.14.186, aarch64 + 32-bit.
- `$ANDROID_ROOT` is `../../..` from this tree, i.e. `/home/rd/lineage`.
- Stock firmware dump for reference and blob extraction:
  `/home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30`
- This tree was originally forked from `crdroidandroid/android_device_realme_RMX2020`
  and later had its sepolicy replaced wholesale from an mt6781 X670 tree
  (`9529c41`). Leftovers from both are a recurring source of bugs — see
  [Dead-config sweeps](#dead-config-sweeps).

Key paths:

| Path | Contents |
|:--|:--|
| `device/infinix/X6812B` | this tree |
| `device/infinix/X6812B-kernel` | prebuilt kernel, DTB, headers |
| `hardware/mediatek` | shared MTK HALs |
| `device/mediatek/sepolicy_vndr` | shared MTK sepolicy (public/private/vendor) |
| `vendor/infinix/X6812B` | generated — never edit |

## Hard rules

These are not negotiable. Breaking them has caused real damage.

1. **Never run a build.** Ask the user to build and report `out/error.log`.
2. **Never propose "rebuild the kernel" as a fix.** There is no kernel source for
   this device. `device/infinix/X6812B-kernel` holds only a prebuilt `Image.gz`,
   DTB and headers, and the 4.14.186 tree is not public. Do not answer a missing
   kernel config option or driver with "rebuild the kernel with CONFIG_X" — it is
   not an available action. Find a userspace or HAL-level solution instead.
3. **Never run `grep`/`rg` at `$ANDROID_ROOT` or on `out/`.** Scope every search
   to a specific directory:
   ```bash
   # correct
   grep -rnF "android.hardware.biometrics.fingerprint@2.1" $ANDROID_ROOT/hardware/interfaces/
   # forbidden
   grep -rn "foo" $ANDROID_ROOT
   ```
4. **Never edit AOSP/Lineage source or `vendor/infinix/X6812B`.** Every fix must
   come from this device tree. The vendor tree is generated output only.
5. **Never `git reset --hard`.** It has already destroyed an uncommitted user
   edit in this tree. Use `git checkout -- <path>` on only the files you touched,
   then `git status --short` to confirm you are not sweeping up user work.
6. **No explanatory comments in files.** Put the reasoning in the commit message.
7. **Before adding an `.rc` service, prop, sepolicy rule, or blob, check whether
   AOSP or MediaTek already provides it.** Duplicates are the most common defect
   in this tree.

## Commands

```bash
# Regenerate the vendor tree after ANY proprietary-files.txt change.
# Must run to completion before building — see Build error triage.
./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30

# Refresh pinned-blob sha1s. Must be idempotent: a second run must leave
# `git diff` empty.
python3 update-sha1sums.py
```

Validation without a build is covered in
[Validate without a full build](#validate-without-a-full-build).

## Validate without a full build

Most build failures come from host tools that are already built. `checkpolicy`,
`check_elf_file` and `build_sepolicy` all live in `out/host/linux-x86/bin/`, so
you can reproduce a failure in seconds and confirm a fix before asking for a
build — which matters because you are not allowed to build.

### General recipe

Every failing ninja rule is written verbatim into `out/error.log` on a
`Command:` line. Most of these commands are directly re-runnable. Extract it
with Python (not `sed`, the line contains quotes, `&&` chains and escaped
newlines):

```bash
python3 - <<'PY'
import re
log = open('out/error.log').read()
m = re.search(r'^Command: /bin/bash -c "(.*)"\s*$', log, re.M)
print(' '.join(m.group(1).split()))
PY
```

Run the result and you get the identical error text without a build. Then, after
changing the tree, re-run the same command to prove the fix. Note that for
soong-generated inputs you must regenerate them first — see below.

### sepolicy

The only soong-generated input is `vendor_sepolicy.conf`, a plain `m4`
invocation. Recover it once from the **split** ninja files (not the single
`build.lineage_X6812B.ninja`, which does not contain the rule):

```bash
python3 - <<'PY'
lines=open('out/soong/build.lineage_X6812B.9.ninja', errors='replace').read().splitlines()
start=next(i for i,l in enumerate(lines) if l.strip()=='# Module:  vendor_sepolicy.conf')
for i in range(start, start+40):
    if lines[i].startswith('    command = '):
        cmd=lines[i].split('command = ',1)[1]
        j=i+1
        while j<len(lines) and lines[j].startswith('        ') and '=' not in lines[j][:20]:
            cmd+=lines[j].strip(); j+=1
        open('/tmp/opencode/run.sh','w').write(cmd.split(' # hash')[0]+'\n')
        break
PY
bash /tmp/opencode/run.sh
out/host/linux-x86/bin/checkpolicy -C -M -c 30 -o /tmp/out.cil.raw \
  out/soong/.intermediates/system/sepolicy/vendor_sepolicy.conf/android_common/X6812B/vendor_sepolicy.conf
```

Repeat the extraction for `# Module:  sepolicy_neverallows.checkpolicy.conf` and
run `checkpolicy -M` on that too. **Unknown types alone will not catch
`neverallow` violations** — you need both.
  (Its `.conf` is under `android_common/sepolicy_neverallows.checkpolicy.conf`,
  with no `<device>` subdirectory.)

Gotchas:

- `checkpolicy` prints `ERROR 'unknown type X'` but **still exits 0**. Read the
  output and confirm the output file was created; never trust the exit code.
- The `.conf` files go stale. After editing any `.te` you **must** re-run `m4`
  or you are validating the old policy.

### VINTF manifest and matrix (checkvintf / assemble_vintf)

Validate device manifest changes, fragment conflicts (duplicate FqInstances),
and framework compatibility matrix (FCM) requirements without a build:

```bash
# 1. Re-assemble vendor manifest and update staging target
BOARD_SEPOLICY_VERS="202404" PRODUCT_ENFORCE_VINTF_MANIFEST=true \
  ../../../out/host/linux-x86/bin/assemble_vintf \
  -i configs/manifests/manifest.xml \
  -o ../../../out/soong/.intermediates/system/libhidl/vintfdata/vendor_manifest.xml/android_arm64_armv8-2a-dotprod_cortex-a75/manifest.xml

cp -f -d ../../../out/soong/.intermediates/system/libhidl/vintfdata/vendor_manifest.xml/android_arm64_armv8-2a-dotprod_cortex-a75/manifest.xml \
  ../../../out/target/product/X6812B/vendor/etc/vintf/manifest.xml

# 2. Check vendor manifest against fragments in /vendor/etc/vintf/manifest/
../../../out/host/linux-x86/bin/checkvintf --check-one \
  --dirmap /vendor:../../../out/target/product/X6812B/vendor \
  --dirmap /apex:../../../out/target/product/X6812B/obj/PACKAGING/check_vintf_all_intermediates/apex \
  --property ro.boot.product.vendor.sku=

# 3. Check full framework/device compatibility matrix
../../../out/host/linux-x86/bin/checkvintf --check-compat \
  --dirmap /system:../../../out/target/product/X6812B/system \
  --dirmap /vendor:../../../out/target/product/X6812B/vendor \
  --dirmap /odm:../../../out/target/product/X6812B/vendor/odm \
  --dirmap /product:../../../out/target/product/X6812B/product \
  --dirmap /system_ext:../../../out/target/product/X6812B/system_ext \
  --dirmap /apex:../../../out/target/product/X6812B/obj/PACKAGING/check_vintf_all_intermediates/apex \
  --property ro.product.first_api_level=30 \
  --property ro.boot.product.hardware.sku= \
  --property ro.boot.product.vendor.sku=
```

Step 2 catches conflicting FqInstances between `manifest.xml` and installed HAL
fragments. Step 3 prints `COMPATIBLE` on success.

### SELinux contexts (checkfc, check_prop_prefix)

Validate context files against `precompiled_sepolicy` to catch invalid regex,
undeclared types, or missing labels without building:

```bash
POLICY="../../../out/target/product/X6812B/vendor/etc/selinux/precompiled_sepolicy"

# File contexts
../../../out/host/linux-x86/bin/checkfc "$POLICY" sepolicy/vendor/file_contexts

# Property contexts syntax & types
../../../out/host/linux-x86/bin/checkfc -p "$POLICY" sepolicy/vendor/property_contexts

# HW service contexts (HIDL)
../../../out/host/linux-x86/bin/checkfc -l "$POLICY" sepolicy/vendor/hwservice_contexts

# Service / vndservice contexts (AIDL / Binder)
../../../out/host/linux-x86/bin/checkfc -s "$POLICY" sepolicy/vendor/service_contexts
../../../out/host/linux-x86/bin/checkfc -v "$POLICY" sepolicy/vendor/vndservice_contexts

# Verify vendor property Treble prefix compliance
../../../out/host/linux-x86/bin/check_prop_prefix \
  --property-contexts sepolicy/vendor/property_contexts \
  --allowed-property-prefix vendor. ro.vendor. persist.vendor. \
  --allowed-context-prefix vendor_ \
  --strict
```

### Init scripts (host_init_verifier)

Catch syntax errors, unrecognized options, illegal trigger lines, and undefined
UID/GID permissions in `.rc` files (note: `ueventd*.rc` uses ueventd format, not init):

```bash
PASSWD="../../../system/core/libcutils/include/private/android_filesystem_config.h"
for rc in rootdir/etc/init/hw/*.rc rootdir/etc/init.recovery.mt6768.rc; do
  [ -f "$rc" ] && ../../../out/host/linux-x86/bin/host_init_verifier -p "$PASSWD" "$rc"
done
```

### XML syntax and schemas (xmllint)

Check XML well-formedness and validate configs against AOSP XSD schemas:

```bash
# Syntax check across configs
../../../out/host/linux-x86/bin/xmllint --noout \
  configs/audio/audio_policy_configuration.xml \
  configs/media/media_codecs.xml \
  configs/manifests/manifest.xml

# Schema validation for media codecs
../../../out/host/linux-x86/bin/xmllint --noout \
  --schema ../../../frameworks/av/media/libstagefright/xmlparser/media_codecs.xsd \
  configs/media/media_codecs.xml
```

### prebuilt ELF dependencies

`check_elf_file` validates that every `DT_NEEDED` of a prebuilt resolves to a
declared `shared_libs` entry. The command in `error.log` is directly re-runnable:

```bash
python3 - <<'PY'
import re
log = open('out/error.log').read()
m = re.search(r'check_elf_file (.*?) \) && \(touch', log, re.S)
open('/tmp/opencode/elfcheck.sh','w').write(
    'out/host/linux-x86/bin/check_elf_file ' + ' '.join(m.group(1).split()) + '\n')
PY
bash /tmp/opencode/elfcheck.sh     # exit 0 = pass, 2 = fail
```

It uses the already-built dependency `.so` files under
`out/target/product/X6812B/obj/SHARED_LIBRARIES/`, so it is valid as long as
those exist from a previous build.

Useful standalone check across the whole vendor tree — does any `DT_NEEDED` name
lack an installed file? Compare against `out/soong/installs-lineage_X6812B.mk`,
which is the authoritative list of what gets installed:

```bash
python3 - <<'PY'
import re, subprocess, glob, os
inst=open('out/soong/installs-lineage_X6812B.mk', errors='replace').read()
known={os.path.basename(m) for m in
       re.findall(r'out/target/product/X6812B/[^\s:]*lib64/([A-Za-z0-9@._+-]+\.so):', inst)}
vdir='vendor/infinix/X6812B/proprietary/vendor/lib64'
known |= {os.path.basename(f) for f in glob.glob(vdir+'/*.so')}
need={}
for f in glob.glob(vdir+'/*.so'):
    out=subprocess.run(['readelf','-d',f],capture_output=True,text=True).stdout
    for n in re.findall(r'Shared library: \[([^\]]+)\]', out):
        need.setdefault(n,set()).add(os.path.basename(f))
for n,w in sorted(need.items()):
    if n not in known: print(n, '<-', sorted(w)[:4])
PY
```

Do **not** compare only within one directory — most dependencies legitimately
come from `/system` or the vendor tree, so a same-directory scan produces a long
list of false positives.

### Test compile without Soong

Individual components can be test-compiled in seconds without waiting for a
full Soong build:

#### 1. Overlays (`aapt2 compile`)
```bash
python3 - <<'PY'
import glob, subprocess, os, tempfile
aapt2 = '../../../out/host/linux-x86/bin/aapt2'
with tempfile.TemporaryDirectory() as tmpdir:
    for ov in sorted(glob.glob('overlay/*')):
        xmls = glob.glob(f'{ov}/res/**/*.xml', recursive=True)
        if xmls:
            res = subprocess.run([aapt2, 'compile', '-o', tmpdir, '--pseudo-localize'] + xmls,
                                 cwd='../../..', capture_output=True, text=True)
            print(f"{ov}: {'OK' if res.returncode == 0 else 'FAIL: ' + res.stderr}")
PY
```

#### 2. C++ Shims (`clang++`)
```bash
prebuilts/clang/host/linux-x86/clang-r536225/bin/clang++ -c \
    device/infinix/X6812B/libshims/libshim_unwindstack.cpp -o /tmp/shim.o \
    -target aarch64-linux-android10000 \
    -Isystem/unwindstack/include -Isystem/core/include -Isystem/libbase/include \
    -std=c++20 -fPIC -Wall -Werror && rm -f /tmp/shim.o
```

#### 3. Blueprint formatting (`bpfmt`)
```bash
# Check formatting without writing (exit 0 if clean)
find . -name Android.bp -exec ../../../prebuilts/build-tools/linux-x86/bin/bpfmt -d {} +

# Auto-format in place
find . -name Android.bp -exec ../../../prebuilts/build-tools/linux-x86/bin/bpfmt -w {} +
```

### Live-test RRO overlays on device

Test resource and overlay changes live on device without rebuilding:

```bash
python3 - <<'PY'
import os, subprocess, tempfile, glob

android_root = os.path.abspath('../../..')
aapt2 = os.path.join(android_root, 'out/host/linux-x86/bin/aapt2')
zipalign = os.path.join(android_root, 'out/host/linux-x86/bin/zipalign')
apksigner = os.path.join(android_root, 'out/host/linux-x86/bin/apksigner')
framework_res = os.path.join(android_root, 'out/target/product/X6812B/system/framework/framework-res.apk')

# 1. Target overlay directory (e.g. overlay/SettingsResTarget or overlay/FrameworksResTarget)
overlay_dir = os.path.abspath('overlay/FrameworksResTarget')
manifest = os.path.join(overlay_dir, 'AndroidManifest.xml')
res_xmls = glob.glob(f"{overlay_dir}/res/**/*.xml", recursive=True)

pem = os.path.join(android_root, 'vendor/lineage-priv/keys/releasekey.x509.pem')
pk8 = os.path.join(android_root, 'vendor/lineage-priv/keys/releasekey.pk8')

with tempfile.TemporaryDirectory() as tmpdir:
    apk_unaligned = os.path.join(tmpdir, 'unaligned.apk')
    apk_aligned = os.path.join(tmpdir, 'aligned.apk')
    out_apk = '/tmp/test_overlay.apk'

    subprocess.run([aapt2, 'compile', '-o', tmpdir, '--pseudo-localize'] + res_xmls,
                   check=True, cwd=android_root)
    flats = glob.glob(f"{tmpdir}/*.flat")
    # Link against framework-res.apk for internal framework attrs/resources;
    # for APEX targets (WifiResTarget, TetheringConfigTarget), also pass
    # -I /tmp/ServiceWifiResources.apk or /tmp/TetheringNext.apk pulled from /apex/.
    subprocess.run([aapt2, 'link', '-o', apk_unaligned, '-I', framework_res,
                    '--manifest', manifest, '--min-sdk-version', '35',
                    '--target-sdk-version', '35', '--keep-raw-values'] + flats,
                   check=True, cwd=android_root)
    subprocess.run([zipalign, '-f', '4', apk_unaligned, apk_aligned], check=True, cwd=android_root)
    subprocess.run([apksigner, 'sign', '--key', pk8, '--cert', pem, '--out', out_apk, apk_aligned],
                   check=True, cwd=android_root)
    print("Built signed overlay at /tmp/test_overlay.apk")
PY

# 2. When /vendor is mounted ro (no remount), push to /data/local/tmp and bind-mount
DEST="/vendor/overlay/FrameworksResTarget.apk"
adb push /tmp/test_overlay.apk /data/local/tmp/test_overlay.apk
adb shell "su -c 'chmod 644 /data/local/tmp/test_overlay.apk && chcon u:object_r:vendor_overlay_file:s0 /data/local/tmp/test_overlay.apk && mount -o bind /data/local/tmp/test_overlay.apk $DEST && stop && start'"

# To revert a bind-mounted overlay cleanly (plain umount fails with EBUSY while system_server holds it):
adb shell "su -c 'umount -l $DEST && rm -f /data/local/tmp/test_overlay.apk && stop && start'"

# 3. Verify idmap on device
adb shell "cmd overlay dump android.overlay.target"
```

- **Live-test `ro.*` props without a full reboot**: use APatch's
  `/data/adb/ap/bin/resetprop` (e.g. `resetprop -d <prop>` or `resetprop <prop> <val>`),
  then restart the consuming native daemon plus Zygote (`stop surfaceflinger && start surfaceflinger && stop && start`).

### Compile and test individual binaries with Ninja (without a full build)

When developing or testing a HAL binary (e.g. `android.hardware.health-service.mediatek`),
you can compile ONLY that binary target using the prebuilt ninja without invoking a full ROM build:

```bash
# 1. Compile the single binary target
../../../prebuilts/build-tools/linux-x86/bin/ninja -f ../../../out/combined-lineage_X6812B.ninja \
  out/soong/.intermediates/hardware/mediatek/aidl/health/android.hardware.health-service.mediatek/android_vendor_arm64_armv8-2a-dotprod_cortex-a75/android.hardware.health-service.mediatek

# 2. Push to live device and set permissions/SELinux context
BIN="../../../out/soong/.intermediates/hardware/mediatek/aidl/health/android.hardware.health-service.mediatek/android_vendor_arm64_armv8-2a-dotprod_cortex-a75/android.hardware.health-service.mediatek"
adb push "$BIN" /vendor/bin/hw/android.hardware.health-service.mediatek
adb shell "chmod 755 /vendor/bin/hw/android.hardware.health-service.mediatek && chcon u:object_r:hal_health_default_exec:s0 /vendor/bin/hw/android.hardware.health-service.mediatek"

# 3. Restart the service and inspect live status/dumpsys
adb shell "killall -9 android.hardware.health-service.mediatek; sleep 1.5; pidof android.hardware.health-service.mediatek"
adb shell "dumpsys battery"
```
### What cannot be fixed without a build

`stem`/`module name` collisions, duplicate install rules and AIDL version
clashes are resolved by soong's module graph, not by a standalone tool. For those
you must reason from `Android.bp` and the installs file, then ask for a build.

## Debugging order

When you hit an error, work in this order. Do not jump to step 5.

1. **Search for an existing fix.** `gh search commits '<error text>'` or the
   GitHub MCP tools. Most MTK/Lineage problems are already solved somewhere.
2. **Read the AOSP source.** Find the exact framework/HAL code, struct layout,
   and log tag that produced the message.
3. **Compare with a sibling device tree.** e.g.
   `mt6768-dev/android_device_xiaomi_mt6768-common`.
4. **Reverse-engineer stock.** Check the stock dump's props, init `.rc`,
   audio/media XMLs, and binaries.
5. **Test live on the device.** Only now start changing things.

The device is ext4 + rw, so you can test hypotheses directly (push a file, edit
a sysfs node, `setprop`) and confirm before committing. If the partition were
erofs, do not force a remount.

## Build error triage

`out/error.log` is **overwritten** by every attempt. Always compare its mtime
against the mtimes of files you changed before trusting it.

### Which stage failed

Two soong stages fail with very different messages — identify which you have:

| Message | Stage | Meaning |
|:--|:--|:--|
| `analyzing Android.bp files and generating ninja` | soong/blueprint | missing src file, bad module, AIDL version clash |
| `writing legacy Make module rules` / `overriding commands for target` | Make install | two modules install to the same destination |

For duplicate installs, grep the target in
`out/soong/installs-lineage_X6812B.mk` — it lists every rule, so you can see
both sources.

### Known traps

- **A stale output file does not mean the current config builds.** A
  `build.lineage_X6812B.ninja` from an earlier successful run will still be on
  disk after a later failure. Check timestamps.
- **`stem:` renames only the installed file, not the module.** Two modules can
  therefore write one destination. Prebuilt blobs whose `DT_NEEDED` is baked in
  (e.g. `libui-v32.so` wants `android.hardware.graphics.common-V2-ndk_platform.so`)
  need the installed name to match, but check whether
  `hardware/lineage/compat` already installs that name before adding a `stem`.
- **Never point a module at a source-built AIDL stub when a pure prebuilt
  exists.** compat's `*-ndk_platform` pulls `...-V2-ndk-source`, while
  `libui -> libgralloctypes` pulls `...-V6-ndk-source`; soong then rejects the
  variant with `depends on multiple versions of the same aidl_interface`. Pure
  `cc_prebuilt_library_shared` / `vndk_prebuilt_shared` avoid this entirely.
- **`extract-files.py` must finish before you build.** A
  `libmtknetutils.so does not exist` error was purely a race — the build started
  42 s before extract finished writing 629 blobs. Re-run extract to completion.
- **A prebuilt ELF check failure usually means a SONAME mismatch, not a missing
  dependency.** The checker compares each `DT_NEEDED` against the `DT_SONAME` of
  every declared `shared_libs` entry
  (`build/make/tools/check_elf_file.py`, `check_dt_needed`), *not* against their
  filenames. So `DT_NEEDED "libui-v32.so" is not specified in shared_libs` fires
  even when `libui-v32` **is** declared, because our module points at AOSP's
  `libui.so` whose SONAME is `libui.so`.

  The SONAME cannot be corrected from this tree: `prebuilts/vndk` is AOSP-owned
  and `cc_prebuilt_library_shared` has no `soname` property. The sanctioned fix,
  per `device/common/generate-android-bp-for-blobs.sh:37`, is
  `check_elf_files: false` for that module. Apply it **per blob** with
  `;DISABLE_CHECKELF` in `proprietary-files.txt` — never by restoring
  `BUILD_BROKEN_PREBUILT_ELF_FILES`, which disables the check for all ~543
  prebuilts and hides real problems.

  Before treating a failure as a SONAME mismatch, confirm the dependency is
  actually installed (see the vendor-tree scan under
  [prebuilt ELF dependencies](#prebuilt-elf-dependencies)). A genuinely missing
  library needs a blob, not a check bypass.

## SELinux

Validate changes with the standalone loop in
[Validate without a full build](#validate-without-a-full-build) — it catches both
unknown types and `neverallow` violations in about a minute.

### Rules that bite

- **`init_daemon_domain(X)` expands to `domain_auto_trans(init, X_exec, X)`**, so
  it references a type named `X_exec`. Never delete a `*_exec` type because a
  literal grep finds no other user — grep for the macro argument instead. Same
  trap applies to `add_hwservice`, `binder_use`, `r_dir_file`.
- **An "unused type" scan based on literal text is unsound.** Let `checkpolicy`
  prove it.
- **Vendor policy cannot reference types declared under
  `device/mediatek/sepolicy_vndr/base/private/`** (that is platform policy).
  e.g. `proc_cl_cam_status` lives there, so a vendor `.te` fails with
  `unknown type`.
- **`vendor_default_prop` is under `treble_sysprop_neverallow`**
  (`system/sepolicy/private/property.te:158`). Granting it to a vendor domain
  compiles but fails neverallow.
- **Versioned app domains are siblings, not children.** `untrusted_app` and
  `untrusted_app_29` are separate types, so `get_prop(untrusted_app, ...)` does
  **not** cover an app with `targetSdkVersion=29`. Use `untrusted_app_all`, which
  `untrusted_app_domain()` populates for every version.
- **Test every `genfscon` / `file_contexts` path on the device first** with
  `ls -Z <path>`. A wrong path never matches and fails silently: our `OTG_CTL`
  entry pointed at the mt6781 `odm/odm:tran_battery` path, so the node stayed
  labelled generic `sysfs` and system_server's grant could never apply.
- **The shared `device/mediatek/sepolicy_vndr` already covers most MTK nodes**
  (charger, batteryinfo, wakeup, extcon, vibrator, block queues). Check there
  before adding an entry.
- **`camerahalserver` runs as `mtk_hal_camera`**, not `hal_camera_default`, and
  `venc_srv` runs as `vpud_native`; put `default_prop` `dontaudit` rules on
  those domains.
- **Dropping `;PRESIGNED` from a vendor/system_ext APK changes its SELinux
  domain** from `untrusted_app_*` to `platform_app` (`certificate: "platform"`),
  so verify `sepolicy/vendor/platform_app.te` has the app's camera/sysfs grants.

## Proprietary blobs

`proprietary-files.txt` and `extract-files.py` are coupled — read both together.

- **Pinning is driven by the `|hash` suffix, not the `- from` comment.**
  `extract_utils` treats any file with a hash as pinned and restores it from the
  vendor tree instead of re-extracting.
- **A pinned file that has a blob fixup needs TWO hashes: `source|fixed-up`.**
  A single hash holding the post-fixup value always mismatches, so the pin
  silently never applies and the log says `fixed up` instead of
  `restored pinned file ...`.
- **Do not put ` - from` in a section header** unless you want
  `update-sha1sums.py` to hash everything beneath it — it keys off that exact
  substring and will also collapse `source|fixup` pairs into one hash.
- **Header attributions must be true.** This file once claimed AEE, Widevine,
  Graphics Hardware and WiFi came from Redmi/realme firmware; every one of those
  blobs exists in the stock X6812B dump and differs only by fixups. Only
  `# Media (Codec2)` is external
  (`mt6768-dev/android_device_xiaomi_mt6768-common`).
- **`restored pinned file with hash X has fixup hash X` in RED is not an error.**
  It is a request to add the second hash and returns `MATCH`. Only `BAD_FIXUP`
  is a real failure.
- **`apktool_patch()` requires at least one pre-existing modified file** in
  the patch: `fixups_blob.py` runs `git add` on existing affected paths and
  `git commit`, which fails with `nothing to commit` on new-file-only patches.

## Dead-config sweeps

Before removing a `.rc` line, sepolicy path, or blob:

1. **Diff against stock first.** Anything identical to stock is not our bug;
   deleting it only adds divergence and noisier future diffs. Only paths
   introduced *by this tree* are suspects.
2. **Test every path on the device**, respecting the fstype —
   `genfscon sysfs` → `/sys`, `genfscon proc` → `/proc`:
   ```bash
   for p in /sys/... /proc/...; do [ -e "$p" ] || echo "MISS $p"; done
   ```
3. **`ueventd` rules for absent partitions are not dead** — ueventd skips them
   (`md1dsp`, `secro`, `misc2`).
4. **`disabled` services are not dead** — check for `start X` triggers first.
   `stp_dump` was only removable after confirming
   `persist.vendor.connsys.coredump.mode` is 0.
5. **Stock vendor `.rc` files still list services whose binaries we do not
   extract** (`bt_dump`, `wifi_dump` in `init_connectivity.rc`). init tolerates
   this; do not patch the rc.

## Subsystem playbooks

### Audio

- Never rely on `dumpsys media_session` or app-level `PlaybackState` to conclude
  audio is playing. Verify with `dumpsys media.audio_flinger`: check
  `Standby: yes/no`, active tracks, written frames, underruns.
- logcat main/system wrap fast when verbose HALs flood them (e.g.
  `powerhal-libperfmgr` `sched_setattr`). Check the audio HAL PID, `dmesg`, and
  direct dumpsys before concluding logs are missing.
- Stock MTK audio uses DTS processing (`libdtsaudio.so`, `mtk_bessound`). In
  AOSP, device effects with `EFFECT_FLAG_TYPE_POST_PROC` can be rejected on
  stream output sessions unless patched or given a software fallback.
- If speaker media or game audio is disproportionately quiet at mid slider
  (e.g. 50%), check `AUDIO_STREAM_MUSIC` `DEVICE_CATEGORY_SPEAKER` in
  `configs/audio/audio_policy_volumes.xml`: AOSP's default curve applies
  -20.8 dB attenuation at step 8 (~9% linear amplitude). Use a balanced
  curve (`<point>1,-4500</point> <point>33,-2500</point> <point>66,-1100</point>`)
  and ensure `ro.vendor.mtk_besloudness_support=1` in `vendor.prop` for DRC.

### Wi-Fi Sharing / Tethering

- On legacy 4.14 kernels without eBPF TC tethering packet forwarding support,
  tethered clients will connect but receive no internet routing if BPF offload
  is active. Set `<bool name="config_tether_enable_bpf_offload">false</bool>`
  in `overlay/TetheringConfigTarget/res/values/config.xml` to force fallback
  to Netfilter/iptables MASQUERADE. Verify with `dumpsys tethering`
  (`enableBpfOffload: false`).
- **Hotspot WPA2/WPA3 (`SECURITY_TYPE_WPA3_SAE_TRANSITION`) failures**:
  - Check `dumpsys wifi | grep -E "SupportedFeatures=|config_wifi_softap_sae_supported"`
    and reproduce cleanly from shell with
    `cmd wifi start-softap "TestAP" wpa3_transition "12345678"` (and `cmd wifi stop-softap`).
  - If `config_wifi_softap_sae_supported` is `false` in `WifiResTarget`,
    `SoftApCapability` omits `SOFTAP_FEATURE_WPA3_SAE` (`SupportedFeatures=43`
    instead of `47`). Settings' Android 14/15 `WifiHotspotSecuritySettings` still
    lets users select WPA2/WPA3 (`SecurityType=2` in
    `/data/misc/apexdata/com.android.wifi/WifiConfigStoreSoftAp.xml`), which
    `ApConfigUtil` then rejects with `Error, SAE requires HAL support`
    (`SoftApState mFailureReason=2`). `wlan_drv_gen4m.ko` and AIDL `hostapd`
    support both `wpa3_transition` and `wpa3` when `config_wifi_softap_sae_supported`
    is `true`.
- **Client connects & gets DHCP IP but has no internet (DNS failure)**:
  - Verify `dnsmasq` and port 53/67 listeners while hotspot is active:
    `ps -eZ | grep dnsmasq`, `ss -tulnp | grep -E ":53|:67"`, and
    `dumpsys netd | grep -iE "dnsmasq|tetherStart|tetherDns"`.
  - `NetworkStack` `DhcpServer` (`IpServer.java`) advertises the downstream
    `ap0` IP as the client DNS server. `netd` (`TetherController.cpp`) only
    launches `dnsmasq` with port-53 DNS forwarding when `usingLegacyDnsProxy=true`
    in `Tethering.java` (maintained via `fuck-bpf` patch
    `packages/modules/Connectivity/0022-Tethering-restore-legacy-DNS-proxy-for-dnsmasq.patch`
    reverting `0019`).
  - Do **not** set `config_tether_enable_legacy_dhcp_server=true` in
    `TetheringConfigTarget` as a workaround: when `usingLegacyDnsProxy=false`,
    `TetherController` still passes `--port=0` (`DNS disabled`), and `dnsmasq`'s
    `192.168.42–47.x` ranges do not match `IpServer`'s randomized `ap0` subnet,
    breaking DHCP lease assignment too.

### USB and Ports

- Kernel 4.14 uses `/sys/class/dual_role_usb/dual-role-usb20` (`musb-hdrc`), NOT
  Linux Type-C connector class (`/sys/class/typec/`).
- Do not use AIDL USB (`android.hardware.usb-service.mediatek`): it relies on
  `/sys/class/typec/` and reports 0 ports on this kernel.
- Use the prebuilt `android.hardware.usb@1.1-service-mediatek`. MTP needs it:
  AOSP's `MtpServer` is FunctionFS-based and opens `/dev/usb-ffs/mtp/ep0`
  (`frameworks/av/media/mtp/MtpDescriptors.h`). This kernel has no
  `CONFIG_USB_F_MTP`, so configfs `mtp.gs0` is a stub and
  `usb_get_function name=mtp` never resolves. MTK implements MTP through the
  legacy `android_usb` gadget (`/dev/mtp_usb`), which only the vendor HAL
  drives. With `dual_role_usb`, `MtpService` logs `Couldn't get control FD!`.
  The prebuilt needs these in `device.mk` because the gadget libs are
  otherwise only pulled in by other services:
  `android.hardware.usb.gadget@1.0`, `@1.1`, `android.hardware.usb@1.0`, `@1.1`.
- Declare `@1.1::IUsb/default` and `@1.1::IUsbGadget/default` in `manifest.xml`.
  The prebuilt installs no VINTF fragment of its own, unlike `dual_role_usb`.
- `device/mediatek/sepolicy_vndr` `base/vendor/file_contexts` already labels
  `android\.hardware\.usb@1\.[0-9]+-service-mediatek` as `mtk_hal_usb_exec`.
- After switching HALs, delete any stale `android.hardware.usb@*` fragment left
  under `out/target/product/X6812B/vendor/etc/vintf/manifest/` before running
  `checkvintf --check-one` — a leftover of the other version conflicts as
  duplicate FqInstance.

### Battery and Charging

- Transsion/MediaTek kernel fuelgauge drivers (`fuelgauged`) report scaled-down
  ticks in sysfs `charge_full_design` (e.g. `231000` instead of `5000000`).
- The true battery design capacity is exported at `/sys/devices/platform/tran_battery/BATTERY_QMAX`
  (`5000`). Label this node as `u:object_r:sysfs_batteryinfo:s0` in
  `sepolicy/vendor/file_contexts` so `hal_health_default` can read it.
- In `hardware/mediatek/aidl/health/Health.cpp`, override `UpdateHealthInfo()` to
  read `BATTERY_QMAX` and populate `batteryFullChargeDesignCapacityUah = 5000000`.

### Display & Refresh Rate

- To expose the 90 Hz high refresh rate toggle in Settings -> Display, overlay
  `<bool name="config_show_smooth_display">true</bool>` in `overlay/SettingsResTarget/res/values/config.xml`.
- Verify availability with `cmd overlay dump com.android.settings.target`.
- To keep 90 Hz active without `min_refresh_rate=90`, BOTH are required:
  1. `<integer name="config_defaultRefreshRate">0</integer>` in
     `overlay/FrameworksResTarget/res/values/config.xml` (leaving it at AOSP's
     default `60` causes `PRIORITY_DEFAULT_RENDER_FRAME_RATE` to vote
     `RenderVote(0, 60)` and cap `primaryRanges` render rate to `60 Hz`).
  2. Do **not** set `ro.surface_flinger.enable_frame_rate_override=false` in
     `configs/props/vendor.prop`: when `false`, `VoteSummary` and
     `RefreshRateSelector` discard physical modes outside the `render` range and
     `Scheduler` sets `defaultTouchTimerValue=0`.
- Verify live state and isolate framework voting from HWC mode switching:
  ```bash
  dumpsys display | grep -E "mActiveMode|mActiveRenderFrameRate|mDesiredDisplayModeSpecs|PRIORITY_"
  dumpsys SurfaceFlinger | grep -E "renderRate=|activeMode=|displayManagerPolicy=|frameRateOverrideConfig="
  # Force 90 Hz bypassing PRIORITY_DEFAULT_REFRESH_RATE (revert with delete afterwards):
  settings put system min_refresh_rate 90
  settings delete system min_refresh_rate
  ```

### Touch gestures / DT2W

- Inspect driver wake keys in the kernel (`Image.gz` disassembly of
  `*_ts_work_func`). MTK panels (Novatek, Focaltech) often emit key 183
  (`KEY_F13`) or 143 (`KEY_WAKEUP`) to `/dev/input/event*` while suspended.
- Without a custom keylayout (`usr/keylayout/mtk-tpd.kl`) Android falls back to
  `Generic.kl`, where 183 maps to `F13` — not marked `WAKE` in
  `PhoneWindowManager` — so the wake event is discarded.
- Verify PowerHAL `powerhint.json`: `DoubleTapToWakeEnable` uses
  `/proc/gesture_function`, requiring `"cc1"` to enable and `"cc2"` to disable.

### Display cutout / status bar padding

- Inspect the physical punch-hole position on the device, then compare
  `FrameworksResTarget` `config_mainBuiltInDisplayCutout` and
  `status_bar_height_portrait` against the stock RRO values.
- In `SystemUIResTarget` check `rounded_corner_content_padding`,
  `status_bar_padding_start`, `status_bar_padding_top`. Excess padding or a
  missing status bar height causes clock/icon clipping or asymmetric alignment.

### Offline charging (`kpoc_charger`)

- MTK `kpoc_charger` is a **32-bit ARM** executable (`system/bin/kpoc_charger`)
  requiring 32-bit `system/lib/libshowlogo.so`. Extract both and set
  `compile_multilib: "32"` in `libshims/Android.bp`.
- Add `PRODUCT_PACKAGES += libsuspend` in `device.mk`.
- `init.mt6768.rc` in `on charger` must start **`vendor.light-default`** (the
  AIDL light HAL name, not `vendor.light`) and chown the LED brightness nodes.
- Remove any `service vendor.charger /system/bin/charger` from
  `init.mt6768.rc` — `kpoc_charger.rc` already defines `service kpoc_charger`
  in `class charger`.
- 32-bit Android 15 `libgui`/`libui` shimming pitfalls:
  1. Returning `sp<T>` in `extern "C"` triggers `-Wreturn-type-c-linkage` under
     `-Werror`. Under ARM32 AAPCS `sp<T>` returns via an sret pointer in `r0`;
     use placement `new (out) sp<T>(...)` with `sp<T>* out` first.
  2. `SurfaceComposerClient.h` has `using gui::LayerMetadata;` inside
     `namespace android`. Do not declare `struct LayerMetadata` in the same TU;
     split `android::LayerMetadata` ctor/vtable symbols into
     `libshim_showlogo_metadata.cpp`.
  3. In `getActiveDisplayConfig`, Android 15 `ui::DisplayMode` has `id` at `+0`
     and `std::vector` at `+20`. Never cast `DisplayConfig*` to
     `ui::DisplayMode*` (stack corruption, 4-byte offset shift); copy
     `resolution`, `dpi`, `refreshRate` explicitly.

### UI jank and power

- Jank: check both 60 Hz and 90 Hz; `dumpsys SurfaceFlinger` (HWC/GPU missed
  frames, phase offsets), `dumpsys gfxinfo <pkg> framestats`
  (UI/RenderThread/GPU), FrameTracker CUJ logs, and process scheduling groups
  (`top-app` vs `foreground`).
- Power: PowerHAL node fds and hint stats
  (`dumpsys android.hardware.power.IPower/default`), CPU/GPU clock scaling
  (`/sys/devices/system/cpu/cpufreq/`, `/sys/kernel/ged/hal/`), EAS/stune
  (`/dev/stune/`). Watch for kernel ABI mismatches, e.g. a 4.14 kernel rejecting
  newer uclamp `sched_setattr` with `E2BIG`/`err=7`.

#### Benchmarking

Use a deterministic sequence, not gestures. `input swipe` opens or closes the
shade depending on the state it is already in, so the frame count varies 4x
between identical runs and no two numbers are comparable:

```bash
cmd statusbar collapse; sleep 0.5
dumpsys gfxinfo com.android.systemui reset >/dev/null
i=0; while [ $i -lt 4 ]; do
  cmd statusbar expand-notifications; sleep 0.9
  cmd statusbar collapse; sleep 0.9; i=$((i+1))
done
dumpsys gfxinfo com.android.systemui | grep -E "Total frames|Janky frames:|percentile"
```

"Achieved frame rate" needs `Total frames rendered` divided by the wall time of
the sequence; the percentiles alone cannot distinguish 40 fps from 90 fps,
because a consistently late surface gets a multi-vsync `FrameDeadline` and is
then reported as *not* janky.

Pitfalls that invalidate a measurement:

- `debug.hwui.renderer` set with `setprop` **sticks for every process started
  afterwards**. A bench run after a `skiavk` experiment measures Vulkan, not the
  shipped default. Confirm the backend with
  `dumpsys gfxinfo <pkg> | grep -m1 Pipeline` before trusting a number. The
  shipped default is OpenGL (`ro.hwui.use_vulkan` is unset/empty).
- Every line of the dump must come from the same process generation. Restarting
  SystemUI resets the counters, so `reset` *after* the restart, wait ~8 s, wake
  the device (`input keyevent KEYCODE_WAKEUP; wm dismiss-keyguard`), and check
  that the first run is not `Total frames rendered: 0`.
- `killall com.android.systemui` and `pidof com.android.systemui` can fail with
  `Operation not permitted` / no output under APatch `su`; `kill -9 <pid>` with
  the PID from `ps` works.
- Killing the power HAL leaves `PowerManagerService` holding a dead binder
  (`PowerHalController: setMode failed ... DEAD_OBJECT`). The first hint after
  the reconnect can be dropped, so give it ~2 s and verify with
  `dumpsys android.hardware.power.IPower/default` that the node's
  `Current Index` matches the hint you expected.

#### MTK CPU and GPU boost nodes

`configs/powerhint.json` runs on `hal_power_default`, and only some MTK nodes
actually do anything. Verified on device by triggering a hint and reading the
node back:

- `/sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq` — **ignored**. PPM
  owns the frequency floor: a LAUNCH hint asking for 1800000/2000000 leaves
  `scaling_min_freq` at 500000/850000.
- `scaling_max_freq` — works, but only caps. The old config capped the little
  cluster to 1625000 on every touch, i.e. it downclocked the cluster that
  scrolls the UI.
- `/proc/perfmgr/boost_ctrl/eas_ctrl/perfserv_{ta,fg,bg}_boost` (`-100..100`) and
  `perfserv_{,ta_,fg_,bg_}uclamp_min` (`0..100`) — the sanctioned boost path,
  same set the stock MTK HAL uses in `vendor/etc/powercontable.xml`. Writes from
  `hal_power_default` take effect.
- `/proc/perfmgr/boost_ctrl/cpu_ctrl/perfserv_freq` — `"<c0min> <c0max> <c1min>
  <c1max>"`, `-1` leaves a value alone. This is the only node that moves the
  reported min frequency, and it is not used by stock.
- `/dev/stune/*/schedtune.prefer_idle` — **cannot** be written by the power HAL
  (`Failed to write to node ... fd: -1`); only `schedtune.boost` is granted via
  the `cgroup` label. The MTK equivalent is
  `/proc/perfmgr/boost_ctrl/eas_ctrl/perfserv_prefer_idle`, which is a
  multi-line per-cgroup array, so do not feed it a single value.
- Every node with `ResetOnInit` needs an explicit `DefaultIndex`. Without one
  libperfmgr logs `Failed to read Node[N]'s DefaultIndex, set to last index` and
  resets the node to its **last** value at HAL start — e.g. a permanent 10 %
  `top-app` `schedtune.boost`.

The stock dump's `vendor/etc/powercontable.xml` is the authoritative reference
for a node's meaning and range (`MaxValue`/`MinValue`/`DefaultValue`).

Current known bottleneck: the notification shade. Its frames take ~25-30 ms
(`p50`) regardless of any power hint or `hwc.min.duration` tried so far.
`dumpsys SurfaceFlinger` shows the wallpaper layer as `CLIENT`-composited while
six layers are visible (HWC runs out of overlays), and the shade's own GPU stage
is 10-15 ms, but the layer count is not the cause: over an opaque app there are
0 CLIENT layers and the GPU stage is unchanged.

#### SurfaceFlinger phase offsets

These are the highest-leverage frame-pacing knob, and the tree had them
inverted. `PhaseOffsets::constructOffsets` in `VsyncConfiguration.cpp`
converts an offset to a duration as:

    sfWorkDuration  = vsyncDuration - sfOffset
    appWorkDuration = vsyncDuration + (sfOffset - appOffset)

So a **negative** `sfOffset` makes `sfWorkDuration > vsyncDuration`, i.e. SF is
given a whole extra frame, and shrinks the app's budget to compensate. The old
tree values at 90 Hz (vsync 11111111 ns) produced `sfWorkDuration` 16.0 ms and
`appWorkDuration` 16.0 ms. AOSP's 90 Hz shape is `sf=+1000000`,
`app=+2000000`, giving 10.1 ms / 21.2 ms - one vsync each, app one frame ahead.
RenderEngine averages 8.5 ms here, so SF needs ~10 ms and the 10.1 ms budget is
the correct one. Verify with
`dumpsys SurfaceFlinger | grep -E "app phase|SF phase|duration:"` after a
restart; it prints the derived durations directly.

Measure these only with `setprop ctl.restart surfaceflinger` plus a ~180 s
settle. Without the settle the first sample is 10-20 points worse and looks
like a regression. `kill -9` on SF is not enough: the props are read at startup,
so the restart has to go through init.

Variants measured, 2 runs each, janky % (shade / launcher / Settings):

    sf=-4888889 app=1333333   (old)   13.01 10.02 / 6.72 5.83 / 22.33 20.39
    sf=-6666666 app=1333333           12.76  9.83 / 7.20 6.72 / 20.95 16.98
    sf=       0 app=2000000           10.41  8.10 / 5.46 5.93 / 16.82 20.00
    sf= 1000000 app=2000000   (now)   10.97  8.23 / 5.15 5.51 / 14.02 20.00

`appOffset` was then swept against the winning `sf=1000000` (Settings is the
worst workload, and `appWorkDuration` is what governs it). None of 1500000,
2500000 or 3000000 beat 2000000; 2500000 and 3000000 are clearly worse:

    app=1500000   9.61  7.95 / 5.13 6.99 / 17.14 15.45
    app=2000000  10.85  7.52 / 5.44 5.56 / 17.76 18.35   (kept)
    app=2500000  11.29  8.23 / 5.93 6.49 / 21.15 16.36
    app=3000000  11.95  8.87 / 5.51 7.89 / 16.67 19.44

`debug.sf.enable_gl_backpressure` must stay `0`. The tree's value is not the
AOSP default (`true`) and turning it on is a large regression, not a tuning
knob: launcher 23.08/19.49 % with `sfLongGpuJankyFrames` 472 in the timestats
run, versus 5.44/5.56 % with it off. The trace does report
`appBufferStuffingJankyFrames` in the hundreds either way, so buffer stuffing
is a symptom of the shade's pacing, not something backpressure can remove.

For the Settings workload, `pm compile -m speed-profile` beats `-m speed`:
interleaved over 3 rounds it wins 3/3 (15.60/14.55/15.89 % vs
16.04/17.92/14.95 %) and the shade is a wash. That is also what the tree asks
for - `pm.dexopt.install=speed-profile` and `pm.dexopt.bg-dexopt=speed-profile`
in `configs/props/system.prop`. A device left in `speed` by a manual
`pm compile` will read a couple of points worse than the shipped state, so
check `dumpsys package com.android.settings | grep -A3 "Dexopt state"` before
trusting a Settings number.

`debug.sf.hwc.min.duration` was re-tested after the offsets were fixed and is a
wash between 3 ms and 5 ms (shade 10.36/6.97 vs 10.97/8.23). It stays at 5 ms
because there is no gain to buy by changing it.

`debug.hwui.use_hint_manager` and `debug.sf.enable_adpf_cpu_hint` do have an
effect, but the trade is not worth taking: interleaved A/B over 3 rounds shows
the HWUI hint manager winning the shade 3/3 (7.25/7.35/7.97 % vs
8.27/7.77/8.11 %) while losing the launcher 3/3 (6.11/6.67/6.84 % vs
5.70/6.58/6.49 %). `true` also needs `target_cpu_time_percent` near the AOSP
default of 70; the tree's 30 is too low.

After all of the above the remaining jank is app-bound, not config-bound: a
Perfetto trace of the transitions shows Settings' main thread `traversal`
averaging 8.1 ms (max 92 ms) on an AOT-`speed` app with uclamp pinned to max,
and 62 of 67 shade-expansion frames landing on an exact 11 ms cadence.

### Camera & Media (`TranssionCamera` / Codec2)

Debugging steps:

1. Separate app crashes from HAL/kernel failures: clear tombstones and logs,
   exercise one mode at a time (`AI CAM`, `VIDEO`, `BEAUTY`, `PORTRAIT`,
   `SUPER NIGHT`, `DOCUMENTS`), then inspect `logcat -b crash -d`,
   `/data/tombstones/`, and `dmesg | grep -E "avc: denied|MTK_V4L2|MTK_VCODEC"`.
2. Isolate Codec2/VPUD from `StagefrightRecorder`: test `screenrecord --time-limit 2 /data/local/tmp/sr.mp4`
   while watching `logcat -b main -d -s VPUD:* C2MtkVenc:* MPEG4Writer:* MediaRecorder:*`.
3. Test APK smali/dex changes live without rebuilding by signing with
   `testkey` and bind-mounting over `/system_ext/app/TranssionCamera/TranssionCamera.apk`,
   then `stop && start`; test vendor `.so` changes with `adb push`, `chcon u:object_r:vendor_file:s0`,
   and `stop camerahalserver && start camerahalserver` (or `killall vpud`).
4. Screen recording green screen: VirtualDisplay buffers (`GraphicBufferSource`,
   usage `0x10300`) will allocate as AFBC if `vpu_runtime_caps` is 0 in
   `android.hardware.graphics.{allocator,mapper}@4.0-impl-mediatek.so`.
   `libdpframework.so` cannot decode AFBC in `DpEngine_RDMA` (`config frame error -9`),
   leaving output YV12 frames zeroed (solid green).
5. Screen recording green line / shadowing: odd multiples of 16 width
   (e.g. 1072x2448) cause gralloc4 to align chroma stride to 16 bytes
   (`uvPitch=544`), while `MTK_V4L2` venc expects `yPitch/2=536`. Keep
   `<Limit name="alignment" value="32x16" />` in `media_codecs_c2.xml` so
   encoders align width to 32 (e.g. 1056x2432).
6. Screen recording crash on start (`Cannot find throughput setting`):
   SystemUI (`ScreenRecordPermissionDialogDelegate.kt`) defaults to HEVC
   if `c2.mtk.hevc.encoder` is declared. MT6768 `libvcodecdrv.so`
   (`HEVC_ThroughputSetting`) has no throughput profile for 90 fps and rejects
   resolutions > 1080p if framerate != 30 fps. Do not declare `c2.mtk.hevc.encoder`
   in `media_codecs_c2.xml` (align with stock, which disabled C2 HEVC encoder);
   keep `config_screenRecorderMaxFramerate` capped to 60 in `SystemUIResTarget`;
   and add `<Feature name="can-swap-width-height"/>` to `c2.mtk.avc.decoder`
   and `encoder` so portrait `1080x2460` does not fail decoder limits and get
   downscaled to `948x2160`.

- `logcat` floods at 30+ fps with `TranCam/Plugin_TranStream` and `Hal3ARaw`;
  always query with `logcat -b main,crash -d -t 500 '*:W'` or `-s <tags>`, and
  check `/data/tombstones/` directly for `camerahalserver` (`MF@CapPipe`) crashes.
- Audit both `System.loadLibrary` (`dexdump`) and `dlopen` strings (`strings`
  for `\.so$` and `/vendor/`) against `proprietary-files.txt`: `TranssionCamera`
  and `libmtkcam_3rdparty.customer.so` load `libanc_hdr.so` (`vendor/etc/anc_siq/*`),
  `libcameracustom.plugin.so` (`libremosaic*`/`librmsclib1.so`), and
  `libcommon.so` (`libXDocProcessSDK.so`) dynamically without `DT_NEEDED`.
- Stock `libsdk_sr.so` has a 4-byte NEON tail over-read (`0x1c8c50: ldr d16, [x8]`)
  that was harmless on Android 11 jemalloc but hits Scudo's guard page on
  Android 15; fixed via `sig_replace` in `extract-files.py`.
- Hybrid `vpud` (stock Android 11 `libvpud_vcodec.so`) + `fire` Android 14
  `libcodec2_mtk_venc.so` breaks `MediaRecorder` (`StagefrightRecorder`) even
  when `screenrecord` and `Aperture` (`MediaMuxer`) succeed: `AP_IPIMSG_ENC_SET_PARAM 15`
  must return `0` (`0x12a2c`: `movs r7, #0`) and `u4TSVC` (`0x11b42`) must be
  zeroed so `VPUD` does not inject a 9-byte `PrefixNALU` rejected by
  `MPEG4Writer::parseAVCCodecSpecificData`.

## Commits

- Commit message structure:
  1. Subject line: `<codename>: <summary>` (e.g. `X6812B: wifi: enable WPA3-SAE softap support`).
  2. Log snippet: verbatim error, logcat trace, or failing command output showing the bug/repro.
  3. Explanation: root cause, what was verified on hardware vs assumed, and why the chosen fix is correct.
- No explanatory comments in code/config files; put all reasoning in the commit message.

## Maintaining this file

Update this file whenever you fix something non-obvious: how to reproduce, how
to diagnose, what you tried that failed, and any mistake you made. Prefer adding
to an existing section over creating a new one. Keep entries actionable — a
command or a concrete check beats a description.
