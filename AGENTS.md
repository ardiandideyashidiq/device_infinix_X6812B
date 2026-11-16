# AGENTS.md

Guidance for coding agents working on the Infinix X6812B (MT6768) device tree.

This file is the whole instruction set: overview, hard rules, host-side validation recipes, the build and
blob traps that already cost time here, live-device rules, and the per-subsystem playbooks. Nothing below
requires another file. `docs/` keeps the longer history behind these sections; it is reference only, and no
rule may live there alone — when you learn something, add it here.

## Project overview

- Target: AxionOS 2.9 (`NOESIS`) on LineageOS 23.2 (Android 16 QPR2, release `bp4a`), userdebug.
- Product: `lineage_X6812B` (Axion keeps the lineage_ product prefix); lunch as
  `axion X6812B userdebug [va|pico|core|gms]`.
- SoC: MediaTek MT6768 (Helio G85 class), kernel 4.14.186, aarch64 + 32-bit.
- `$ANDROID_ROOT` is `../../..` from this tree (`/home/rd/axion`).
- Stock dump: `/home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30`
- Reference tree: `/home/rd/Projects/android/custom-rom/x670/android_device_infinix_X670`
  (mt6781 — useful, but its sysfs paths and sepolicy labels are not ours).
- Tree history: forked from `realme_RMX2020`, sepolicy replaced from mt6781 X670 (`9529c41`).

Key paths:
- `device/infinix/X6812B` (this tree) | `device/infinix/X6812B-kernel` (prebuilt Image.gz, DTB, headers)
- `hardware/mediatek` (shared MTK HALs) | `device/mediatek/sepolicy_vndr` (shared MTK sepolicy)
- `device/axion/common` (Axion product config; `config/version.mk` holds the 2.9 version props)
- `hardware/lineage/compat` (Lineage compat shims and `*-vNN` prebuilts — check here before adding one)
- `vendor/lineage` (Axion fork of the Lineage vendor tree — product inheritance lives here)
- `vendor/infinix/X6812B` (generated — never edit)
- `out/error.log` (the user's build failure report, overwritten by every attempt)

## Hard rules (Non-negotiable)

1. **Never run a build, and never bootstrap one.** Ask the user to build and report `out/error.log`.
   This covers `m`, `mm`, `mmm`, `mka`, `soong_ui`, `ninja`,
   `prebuilts/build-tools/linux-x86/bin/ninja`, `source build/envsetup.sh`, `lunch`, `choosecombo`, and
   any `OUT_DIR=`-redirected variant. Running `m` here stalls after "Running globs..." (the
   `out/.ninja_fifo` frontend is never drained) and a redirected `OUT_DIR` leaves a half-built tree the
   user has to delete. A small-looking target does not make it allowed. Use "Validate without a build".
2. **Never propose "rebuild the kernel" as a fix.** Kernel source is unavailable; prebuilt only.
3. **Never run `grep`/`rg` at `$ANDROID_ROOT` or over `out/`.** Scope to a named file or subdirectory;
   prefer `search_files`. A broad recursive search of the whole source tree is refused and wasted.
4. **Never edit AOSP/Lineage/Axion source or `vendor/infinix/X6812B`.** Fixes belong in this tree; when a
   fix can only land upstream, say so and stop. One exception, only when the user asks: the `fuck-bpf`
   series, applied with `/home/rd/axion/fuck-bpf/apply.py --mb` from the source root (its BPF reverts are
   what let Android 16 QPR2 boot past `bpfloader-failed` on this 4.14 kernel). Never edit those repos by hand.
5. **Never `git reset --hard`.** Use `git checkout -- <path>` only on files you touched.
6. **No explanatory comments in code/config files.** Put the reasoning in the commit message.
7. **Check AOSP, MediaTek and Axion upstream (and the stock dump) before adding anything.** No duplicate
   configs/props/services; no new file that stock already provides.
8. **Verify a prebuilt or shim fix host-side before the user builds again.** Never ask for a build to test
   a compile error or an ELF check you can reproduce locally. See "Rebuild one C++ source by hand" and
   "prebuilt ELF dependencies".
9. **Subsystem work requires reading the matching playbook section first** (Audio, Wi-Fi/Tethering, Bluetooth,
   USB, Telephony/RIL, Battery, Offline charging, Sensors, Display/Refresh, Touch/DT2W, UI jank/Power,
   Camera/Media). Each section opens with the trap that is already known on this device.
10. **Validate host-side before the device sees it.** Manifest, property, `file_contexts`, `.rc`, ELF and
    XML edits all have a standalone validator below. `xmllint --noout` proves well-formed XML, not a
    bootable manifest.
11. **Never overwrite a working system file wholesale, and never restart the framework to "test" something.**
    Stage with `mount --bind`, reboot to apply. `ctl.restart hwservicemanager` / `zygote` / `stop; start`
    take down display and framework with no live recovery. Read "Live device testing" before any device write.
12. **Significant resource tasks need the user's explicit permission first**: `extract-files.py` runs,
    full-image unpacks, vendor-tree regeneration. Ask; do not start one because it is convenient.
13. **Leave nothing behind.** Scratch objects, linked libraries and scripts go under `out/<scratch>/` and
    are deleted in the same turn. Report any artifact you created that outlives the task.

## Debugging workflow (Order of attack)

1. **Search for an existing fix.** `gh search commits '<error text>'` / `search_commits` / GitHub MCP.
   Most MTK and Lineage problems are already solved somewhere.
2. **Read the source that emitted the message.** AOSP / MediaTek / Axion: exact log tag, struct layout,
   service name.
3. **Compare sibling trees**: `mt6768-dev/android_device_xiaomi_mt6768-common`, the X670 reference tree,
   `device/axion/common`.
4. **Reverse-engineer the stock dump**: props, `.rc`, XMLs, ELF blobs. Stock is the ground truth for what
   this hardware actually does.
5. **Test live on the device** — only now, following "Live device testing".

Fast triage rules:
- **`adb root` vs `adb unroot` on user builds (`ro.build.type=user`)**:
  Running `adb root` switches adbd to `u:r:su:s0`. On user builds, `allow domain su:binder { call transfer }`
  is stripped by the `userdebug_or_eng()` macro. Binder transactions to vendor HALs (e.g. `mtk_hal_c2`,
  Codec2, `scrcpy`) fail with `Check failed: transStatus.isOk() Codec2 AIDL service "default" does not have IConfigurable`
  due to `avc: denied { transfer } for scontext=u:r:mtk_hal_c2:s0 tcontext=u:r:su:s0`. Run `adb unroot` to return
  the client context to `u:r:shell:s0`.
- **Dumpsys pipe denials (`FAILED_TRANSACTION`)**:
  `dumpsys <service>` passes a pipe fd (`shell:fifo_file` or `dumpstate:fifo_file`) to the service's dump
  method. HAL domains (such as `hal_power_default`) fail dumpsys with `FAILED_TRANSACTION` unless granted
  `allow <domain> { dumpstate shell }:fifo_file write;`.
- **Focused window check**:
  Never assume an app's main activity is rendering. Always check `dumpsys window | grep -E "mCurrentFocus|mFocusedApp"`.
  E.g. Settings may have `com.android.settings.intelligence/.search.SearchActivity` focused instead of
  `com.android.settings/.homepage.SettingsHomepageActivity`.
- **AVC denial inspection**:
  Query `logcat -d | grep avc | tail -n 25` or `logcat -d -b crash` rather than `dmesg`, which is restricted
  to root on user builds.
- **RRO & Overlay live testing without reboot (`cmd overlay fabricate`)**:
  Testing overlay resource edits (`isStatic="true"` or dynamic) instantly on device without a build or reboot:
  ```bash
  su -c "cmd overlay fabricate --target <target_pkg> --name <Name> <target_pkg>:<type>/<res_name> 0x12 <1|0>"
  cmd overlay enable com.android.shell:<Name>
  cmd overlay lookup <target_pkg> <target_pkg>:<type>/<res_name>
  ```
  Type `0x12` is boolean (`TYPE_INT_BOOLEAN`), `0x1c` is ARGB color (`TYPE_INT_COLOR_ARGB8`). Inspect full idmap
  and target package mappings with `cmd overlay dump <package.target>`.
- **`content://` vs `file:///` URIs in Android 16 testing**:
  `am start -a android.intent.action.VIEW -d file:///...` is blocked by `FileUriExposedException` or ignored
  by modern gallery/music players. Always index with MediaStore first:
  ```bash
  am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE -d file://<path>
  content query --uri content://media/external/{video,audio}/media --projection _id
  am start -a android.intent.action.VIEW -d content://media/external/{video,audio}/media/<id> -t <mime> -n <component>
  ```
  Note: `content query --projection` column names MUST be separated by spaces, not commas (commas throw
  `IllegalArgumentException: Invalid column _id,_display_name`).



`out/error.log` is **overwritten by every build attempt**; compare its mtime against the files you changed
before trusting it. Each failing rule is quoted verbatim on a `Command:` line. Extract it with Python
(not `sed` — the line contains quotes, `&&` chains and escaped newlines):

```bash
python3 - <<'PY'
import re
log = open('out/error.log').read()
m = re.search(r'^Command: /bin/bash -c "(.*)"\s*$', log, re.M)
print(' '.join(m.group(1).split()) if m else 'no bash-wrapped Command: line')
PY
```

Two soong stages fail with very different messages — identify which one you have before reading further:

| Message | Stage | Meaning |
|:--|:--|:--|
| `analyzing Android.bp files and generating ninja` | soong/blueprint | missing src file, bad module, AIDL version clash |
| `writing legacy Make module rules` / `overriding commands for target` | Make install | two modules install to the same destination |

For duplicate installs, grep the destination in `out/soong/installs-lineage_X6812B.mk` — it lists every
rule, so both sources are visible.

## Validate without a build

The host tools are already built: `checkpolicy`, `check_elf_file`, `checkfc`, `host_init_verifier`,
`checkvintf`, `assemble_vintf`, `check_prop_prefix`, `xmllint`, `aapt2` in `out/host/linux-x86/bin/`;
`bpfmt`, `soong_zip` in `prebuilts/build-tools/linux-x86/bin/`. Re-running the quoted `Command:` line
reproduces a failure in seconds and proves a fix — this is the only proof available to you, because you are
not allowed to build.

What genuinely cannot be checked host-side: `stem`/module-name collisions, duplicate install rules, AIDL
version clashes, and symbols satisfied only after soong's variant/suffix machinery runs. For those, reason
from `Android.bp` and the installs file, then ask for a build. Prefer a narrow, trusted check over a broad
one: a validator that cries wolf gets ignored, which is worse than not having it.

### sepolicy

`scripts/check-sepolicy.sh` locates `$ANDROID_ROOT`, finds the `m4` rule for both `vendor_sepolicy.conf`
and `sepolicy_neverallows.checkpolicy.conf` in the split ninja files, runs them and `checkpolicy`s each.
Generated rules land in `out/` and are regenerated every run, so a `.te` edit never leaves stale rules.

```bash
scripts/check-sepolicy.sh
```

- Both configs are compiled because **unknown types alone do not catch `neverallow` violations**.
  `neverallows` has no `<device>` subdirectory.
- `checkpolicy` prints `ERROR 'unknown type X'` but **still exits 0**. Treat an empty output file as
  failure; never trust the exit code. By hand, check `-s`/`-f` on the `.cil` yourself.
- The rule block moves between `build.lineage_X6812B.*.ninja` splits across builds — the script globs them
  all rather than hardcoding one. A missing rule means `out/` is stale (no successful configure since the
  module was last touched), and the script says so instead of silently passing.

### SELinux rules that bite

- **`init_daemon_domain(X)` expands to `domain_auto_trans(init, X_exec, X)`**, so it references a type
  named `X_exec`. Never delete a `*_exec` type because a literal grep finds no user — grep for the macro
  argument. Same for `add_hwservice`, `binder_use`, `r_dir_file`. An "unused type" scan based on literal
  text is unsound; let `checkpolicy` prove it.
- **Vendor policy cannot reference types declared under `device/mediatek/sepolicy_vndr/base/private/`**
  (that is platform policy) — e.g. `proc_cl_cam_status` lives there, so a vendor `.te` fails with
  `unknown type`.
- **`vendor_default_prop` is under `treble_sysprop_neverallow`** (`system/sepolicy/private/property.te:158`).
  Granting it to a vendor domain compiles but fails neverallow.
- **Versioned app domains are siblings, not children.** `get_prop(untrusted_app, ...)` does not cover an
  app with `targetSdkVersion=29`; use `untrusted_app_all`, which `untrusted_app_domain()` populates for
  every version.
- **Test every `genfscon`/`file_contexts` path on the device first** with `ls -Z <path>`. A wrong path never
  matches and fails silently: the `OTG_CTL` entry pointed at the mt6781 `odm/odm:tran_battery` path, so the
  node stayed generic `sysfs` and system_server's grant could never apply. On MT6768 the Mali device is
  `/sys/devices/platform/13040000.mali`; `13000000` is `mfg_cfg`, so copying the X670/X6815D path breaks
  GPU labelling here.
- **`device/mediatek/sepolicy_vndr` already covers most MTK nodes** (charger, batteryinfo, wakeup, extcon,
  vibrator, block queues). Check there before adding an entry.
- **`camerahalserver` runs as `mtk_hal_camera`**, not `hal_camera_default`, and `venc_srv` runs as
  `vpud_native`; put `default_prop` `dontaudit` rules on those domains.
- **Dropping `;PRESIGNED` from a vendor/system_ext APK changes its SELinux domain** from
  `untrusted_app_*` to `platform_app` (`certificate: "platform"`), so verify
  `sepolicy/vendor/platform_app.te` carries the app's camera/sysfs grants.

Context files validate against the built policy without a build:

```bash
POLICY="../../../out/target/product/X6812B/vendor/etc/selinux/precompiled_sepolicy"
../../../out/host/linux-x86/bin/checkfc "$POLICY" sepolicy/vendor/file_contexts
../../../out/host/linux-x86/bin/checkfc -p "$POLICY" sepolicy/vendor/property_contexts
../../../out/host/linux-x86/bin/checkfc -l "$POLICY" sepolicy/vendor/hwservice_contexts
../../../out/host/linux-x86/bin/checkfc -s "$POLICY" sepolicy/vendor/service_contexts
../../../out/host/linux-x86/bin/checkfc -v "$POLICY" sepolicy/vendor/vndservice_contexts
../../../out/host/linux-x86/bin/check_prop_prefix \
  --property-contexts sepolicy/vendor/property_contexts \
  --allowed-property-prefix vendor. ro.vendor. persist.vendor. \
  --allowed-context-prefix vendor_ --strict
```

### VINTF manifest and matrix

```bash
# 1. Re-assemble the vendor manifest and stage it
BOARD_SEPOLICY_VERS="202404" PRODUCT_ENFORCE_VINTF_MANIFEST=true \
  ../../../out/host/linux-x86/bin/assemble_vintf \
  -i configs/manifests/manifest.xml \
  -o ../../../out/soong/.intermediates/system/libhidl/vintfdata/vendor_manifest.xml/android_arm64_armv8-2a-dotprod_cortex-a75/manifest.xml
cp -f -d ../../../out/soong/.intermediates/system/libhidl/vintfdata/vendor_manifest.xml/android_arm64_armv8-2a-dotprod_cortex-a75/manifest.xml \
  ../../../out/target/product/X6812B/vendor/etc/vintf/manifest.xml

# 2. Manifest vs installed fragments — catches conflicting FqInstances
../../../out/host/linux-x86/bin/checkvintf --check-one \
  --dirmap /vendor:../../../out/target/product/X6812B/vendor \
  --dirmap /apex:../../../out/target/product/X6812B/obj/PACKAGING/check_vintf_all_intermediates/apex \
  --property ro.boot.product.vendor.sku=

# 3. Full framework/device compatibility matrix — prints COMPATIBLE on success
../../../out/host/linux-x86/bin/checkvintf --check-compat \
  --dirmap /system:../../../out/target/product/X6812B/system \
  --dirmap /vendor:../../../out/target/product/X6812B/vendor \
  --dirmap /odm:../../../out/target/product/X6812B/vendor/odm \
  --dirmap /product:../../../out/target/product/X6812B/product \
  --dirmap /system_ext:../../../out/target/product/X6812B/system_ext \
  --dirmap /apex:../../../out/target/product/X6812B/obj/PACKAGING/check_vintf_all_intermediates/apex \
  --property ro.product.first_api_level=30 \
  --property ro.boot.product.hardware.sku= --property ro.boot.product.vendor.sku=
```

After switching a HAL version, delete any stale `android.hardware.*@*` fragment left under
`out/target/product/X6812B/vendor/etc/vintf/manifest/` before `--check-one` — the other version otherwise
conflicts as a duplicate FqInstance.

### Init scripts and XML

```bash
PASSWD="../../../system/core/libcutils/include/private/android_filesystem_config.h"
for rc in rootdir/etc/init/hw/*.rc rootdir/etc/init.recovery.mt6768.rc; do
  [ -f "$rc" ] && ../../../out/host/linux-x86/bin/host_init_verifier -p "$PASSWD" "$rc"
done

../../../out/host/linux-x86/bin/xmllint --noout \
  configs/audio/audio_policy_configuration.xml configs/media/media_codecs.xml configs/manifests/manifest.xml
../../../out/host/linux-x86/bin/xmllint --noout \
  --schema ../../../frameworks/av/media/libstagefright/xmlparser/media_codecs.xsd configs/media/media_codecs.xml
../../../out/host/linux-x86/bin/xmllint --noout \
  --schema ../../../frameworks/av/media/libmedia/xsd/media_profiles.xsd configs/media/media_profiles_V1_0.xml
```

`ueventd*.rc` uses ueventd format, not init — `host_init_verifier` will reject it, so skip those paths.

### prebuilt ELF dependencies and SONAME

`check_elf_file` validates that every `DT_NEEDED` of a prebuilt resolves to a declared `shared_libs` entry,
comparing against the **`DT_SONAME` of the declared module, not its filename**.

```bash
python3 - <<'PY'
import re
m = re.search(r'^Command: (.*)$', open('out/error.log').read(), re.M)
open('/tmp/elfcheck.sh', 'w').write(' '.join(m.group(1).split()) + '\n')
PY
bash /tmp/elfcheck.sh        # exit 0 = pass, 2 = fail
```

- **A prebuilt ELF check failure usually means a SONAME mismatch, not a missing dependency.**
  `DT_NEEDED "libui-v32.so" is not specified in shared_libs` fires even when `libui-v32` **is** declared,
  because our module points at AOSP's `libui.so`, whose SONAME is `libui.so`.
- The SONAME cannot be corrected from this tree (`prebuilts/vndk` is AOSP-owned,
  `cc_prebuilt_library_shared` has no `soname` property). The sanctioned fix, per
  `device/common/generate-android-bp-for-blobs.sh:37`, is `check_elf_files: false` for that module —
  applied **per blob** with `;DISABLE_CHECKELF` in `proprietary-files.txt`, then re-run `extract-files.py`.
  Never restore `BUILD_BROKEN_PREBUILT_ELF_FILES`, which disables the check for all ~543 prebuilts and
  hides real problems.
- Before calling a failure a SONAME mismatch, confirm the dependency is actually installed. A genuinely
  missing library needs a blob, not a check bypass.
- For `Unresolved symbol:` failures, list both sides with
  `prebuilts/clang/host/linux-x86/clang-r563880c/bin/llvm-nm -D --defined-only <lib>` /
  `--undefined-only <prebuilt>` and diff the mangled names. A symbol present in the old tree but gone from
  the new one needs a shim in this tree.
- `allow_undefined_symbols: true` only silences `check_elf_file`; it does not fix a symbol that also fails
  to resolve at runtime. Never use it as the fix for a real gap.
- `check_elf_file` stops at the first unresolved symbol and reports a batch, not the whole set — re-run it
  after each fix and expect symbols from sibling shim objects (e.g. `libshim_showlogo_metadata.cpp`) to
  surface once the first real one is gone.

Vendor-tree scan for `DT_NEEDED` names that are never installed. Compare against
`out/soong/installs-lineage_X6812B.mk`, which is authoritative — **do not** compare within one directory,
most deps legitimately come from `/system`:

```bash
python3 - <<'PY'
import re, os, glob, subprocess
inst = open('out/soong/installs-lineage_X6812B.mk', errors='replace').read()
known = {os.path.basename(m) for m in
         re.findall(r'out/target/product/X6812B/[^\s:]*lib64/([A-Za-z0-9@._+-]+\.so):', inst)}
vdir = 'vendor/infinix/X6812B/proprietary/vendor/lib64'
known |= {os.path.basename(f) for f in glob.glob(vdir + '/*.so')}
need = {}
for f in glob.glob(vdir + '/*.so'):
    out = subprocess.run(['readelf', '-d', f], capture_output=True, text=True).stdout
    for n in re.findall(r'Shared library: \[([^\]]+)\]', out):
        need.setdefault(n, set()).add(os.path.basename(f))
for n, w in sorted(need.items()):
    if n not in known:
        print(n, '<-', sorted(w)[:4])
PY
```

SONAME audit (`DT_SONAME != filename` breaks `check_elf_files`):

```bash
python3 - <<'PY'
import os, glob, subprocess
for f in glob.glob('vendor/infinix/X6812B/proprietary/vendor/**/*.so', recursive=True):
    out = subprocess.run(['readelf', '-d', f], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if 'SONAME' in line:
            soname = line.split('[')[1].split(']')[0]
            if soname != os.path.basename(f):
                print(f'SONAME mismatch: {os.path.basename(f)} (SONAME: {soname}) in {f}')
PY
```

A mismatch on a blob that is `dlopen`ed (e.g. `libmmc5603_x6812.so` carrying `DT_SONAME "libmemsic.so"`)
is fixed by appending `;DISABLE_CHECKELF` to its line in `proprietary-files.txt` and re-running
`extract-files.py`.

### Rebuild one C++ source by hand (shims, HAL TUs)

1. Re-run the failing `clang++ -c` command from the `Command:` line, with `-o` pointed at a scratch object
   under `out/<scratch>/` and `-MD -MF <depfile>` stripped. Nothing else needs changing. Readings:
   non-zero rc is the real compile error, verbatim, without a build.
   Read the header that actually declares the function you are shimming (`Basename`/`Dirname` live in
   `android-base/file.h`, not `strings.h`) — a wrong include produces "no member named ..." and costs the
   user another rebuild round-trip.
2. Link explicitly. `clang++` with `-nostdlib++` adds no `-lc`, and `-R`/`-L` does not pull libraries in
   for symbol resolution, so list every dependency:

```bash
prebuilts/clang/host/linux-x86/clang-r563880c/bin/ld.lld -shared --no-undefined \
  -soname <name> -o out/<scratch>/<name>.so out/<scratch>/<obj>.o <dep .so paths...>
```

3. Run `check_elf_file` on the prebuilt with the rebuilt shim passed as `--shared-lib` in place of the
   stale one in `out/soong/.intermediates/`. The flag takes two dashes.
4. Soong's own link step wraps objects in a `soong_zip` archive. `ld.lld` cannot read that zip
   (`unknown directive: PK`) — link the raw `.o` by hand, and only pass a zip to `check_elf_file` with
   `--skip-bad-elf-magic` when you have no other choice.

Shim rules earned the hard way: `libbase` keeps only the `string_view` overloads; the `std::string` ones
live in `hardware/lineage/compat`'s `libbase_shim`, so any shim that calls them must list `libbase` in its
`shared_libs`, and the prebuilt must already `DT_NEEDED libbase.so` for the symbol to resolve at runtime.

### Overlays and Blueprint formatting

```bash
# aapt2 compile every overlay
python3 - <<'PY'
import glob, subprocess, tempfile
with tempfile.TemporaryDirectory() as tmp:
    for ov in sorted(glob.glob('overlay/*')):
        xmls = glob.glob(f'{ov}/res/**/*.xml', recursive=True)
        if xmls:
            r = subprocess.run(['../../../out/host/linux-x86/bin/aapt2', 'compile', '-o', tmp,
                                '--pseudo-localize'] + xmls, cwd='../../..',
                               capture_output=True, text=True)
            print(f"{ov}: {'OK' if r.returncode == 0 else 'FAIL: ' + r.stderr}")
PY

# Blueprint formatting: -d checks (exit 0 = clean), -w rewrites in place
find . -name Android.bp -exec ../../../prebuilts/build-tools/linux-x86/bin/bpfmt -d {} +
```

To test a signed overlay APK live: `aapt2 link` against
`out/target/product/X6812B/system/framework/framework-res.apk` (add `-I` for the APEX targets
`WifiResTarget`/`TetheringConfigTarget` from `/apex/`), `zipalign -f 4`, `apksigner sign` with
`vendor/lineage-priv/keys/releasekey.{x509.pem,pk8}`, then follow the bind-mount recipe below.

### Pre-build checklist (under a minute, before asking for a build)

Run all three after any change to `proprietary-files.txt`, `device.mk` or a regenerated vendor `Android.bp`.

1. **Module-name collision** — soong resolves this last and it aborts the whole build.
   `hardware/lineage/compat` and `prebuilts/vndk` both define modules a vendor tree is likely to redefine.
   Match `name:` at low indentation; a bare `name:` regex also matches `filename:` keys and invents
   duplicates. Run from this tree root:

```bash
python3 - <<'PY'
import re
def bp_names(p):
    return set(re.findall(r'^\s{0,8}name:\s*"([^"]+)"', open(p, errors='replace').read(), re.M))
compat = bp_names('../../../hardware/lineage/compat/Android.bp')
for bp in ('Android.bp', 'libshims/Android.bp'):
    print(bp, 'collisions:', sorted(bp_names(bp) & compat) or 'none')
PY
```

   Any hit is a real error of the form
   `MODULE.TARGET.SHARED_LIBRARIES.<name> already defined by hardware/lineage/compat`. Fix it by dropping
   the extraction and letting the compat module win, unless the dump blob genuinely differs — check with
   `readelf -d` on both first.
2. **Replay the exact `check_elf_file` command** (see above). Catches a `DT_NEEDED` missing from
   `shared_libs` and a compat module that under-declares its dependencies — the latter is upstream
   breakage you cannot fix in this tree, so know before the build.
3. **SONAME audit** (see above) for any newly extracted blob.

If a check names a module in `hardware/lineage/compat` or `prebuilts/vndk`, the fix is not in this tree.
Decide up front whether to work around it at the device level (link a different, correctly-declared
version) or accept it, and say so before the build instead of discovering it hours in.

## Build and module traps

- **A stale output file does not mean the current config builds.** A `build.lineage_X6812B.ninja` from an
  earlier success survives a later failure. Check timestamps.
- **`stem:` renames only the installed file, not the module.** Two modules can write one destination.
  Prebuilt blobs with baked-in `DT_NEEDED` (e.g. `libui-v32.so` wants
  `android.hardware.graphics.common-V2-ndk_platform.so`) need the installed name to match — but check
  whether `hardware/lineage/compat` already installs that name before adding a `stem`.
- **Never point a module at a source-built AIDL stub when a pure prebuilt exists.** compat's
  `*-ndk_platform` pulls `...-V2-ndk-source` while `libui -> libgralloctypes` pulls `...-V6-ndk-source`;
  soong then rejects the variant with `depends on multiple versions of the same aidl_interface`. Pure
  `cc_prebuilt_library_shared` / `vndk_prebuilt_shared` avoid this entirely.
- **`extract-files.py` must finish before you build.** A `libmtknetutils.so does not exist` error was
  purely a race: the build started 42 s before extract finished writing 629 blobs.
- Android 16 reparented swaths of the tree (sepolicy vndr, shared MTK HALs, AIDL migration of HALs that
  were HIDL on 22.2). Treat every 22.2-era `PRODUCT_PACKAGES` entry and `.rc` service as unverified until
  it resolves in the new source.

## Proprietary blobs

`proprietary-files.txt` and `extract-files.py` are coupled — read both together.

- **Pinning is driven by the `|hash` suffix, not the `- from` comment.** Any file with a hash is treated as
  pinned and restored from the vendor tree instead of re-extracted.
- **A pinned file with a blob fixup needs TWO hashes: `source|fixed-up`.** A single post-fixup hash always
  mismatches, the pin silently never applies, and the log says `fixed up` instead of
  `restored pinned file ...`.
- **Do not put ` - from` in a section header** unless you want `update-sha1sums.py` to hash everything
  beneath it — it keys off that exact substring and collapses `source|fixup` pairs into one hash.
- **Header attributions must be true.** The file once claimed AEE, Widevine, Graphics Hardware and WiFi came
  from Redmi/realme firmware; every one of those blobs exists in the stock X6812B dump and differs only by
  fixups. Only `# Media (Codec2)` is external (`mt6768-dev/android_device_xiaomi_mt6768-common`).
- **`restored pinned file with hash X has fixup hash X` in RED is not an error** — it asks for the second
  hash and returns `MATCH`. Only `BAD_FIXUP` is a real failure.
- **`apktool_patch()` requires at least one pre-existing modified file** in the patch: `fixups_blob.py`
  runs `git add` on existing paths and `git commit`, which fails with `nothing to commit` on new-file-only
  patches.

## Live device testing

The device is the user's daily driver. Live testing is how this tree gets verified, but every write needs a
revert path known *before* the write.

- **Back out first.** `adb pull` the current file into the workspace and record its sha256; the revert must
  be a one-liner.
- **Never overwrite a working system file wholesale.** Change the specific line (`sed -i '/pattern/d'`, a
  targeted edit). A well-formed but wrong whole-file push costs a data format.
- **Prefer `/data/local/tmp` + `mount --bind`** over writing into `/vendor` for anything bind-mountable
  (manifest, props, JSON configs, effect XMLs). The mount lives in memory: reboot discards it, `umount` is
  instant, it cannot brick boot. Revert a busy mount with `umount -l`.
- **Verify on the device, not on a host copy** — `adb shell grep <new-string> <path>`. A host-side pull can
  be stale; the parse happens in-guest.

| File | Parsed by | Live edit effective after |
| --- | --- | --- |
| `/vendor/etc/vintf/manifest.xml` | `hwservicemanager`, `libvintf` clients | reboot (the only safe test) |
| `/vendor/etc/vintf/compatibility_matrix.xml` | `checkvintf` at build, framework at boot | reboot |
| `*.rc` under `/vendor/etc/init`, `/system/etc/init` | `init` | reboot (`restart <svc>` reuses the parsed definition) |
| `.prop`, `build.prop` | init property service at first boot | reboot, or `resetprop` + restart the consumer |
| Overlay APKs, `audio_effects.xml`, `powerhint.json` | the owning service | restart that service |

`adb root` + `adb remount` reports "Now reboot your device for settings to take effect": overlayfs merges
new content into `/vendor` on the **next** boot, not immediately. `ro.*` props can be tested without a
reboot via APatch's `/data/adb/ap/bin/resetprop` (`resetprop <prop> <val>`, `resetprop -d <prop>`), then
restart the consuming daemon.

Live testing without reboot (APatch tools in `/data/adb/ap/bin/`):
- **Live SELinux policy injection (`apd sepolicy --live`)**:
  This kernel cannot be set to permissive (`setenforce 0` fails with `Invalid argument`). Instead of rebooting
  or getting blocked on denials during testing, inject rules directly into the kernel's live policy:
  ```bash
  su -c '/data/adb/ap/bin/apd sepolicy --live "allow <scontext> <tcontext> <tclass> { <perms> }"'
  ```
- **Bind-mounting executables across filesystems**:
  - `/data` is mounted with the `nosuid` flag. Executing a daemon bind-mounted from `/data/` causes init
    domain transitions to fail with `avc: denied { nosuid_transition } for scontext=u:r:init:s0 tcontext=u:r:<hal>:s0 tclass=process2`.
  - Staging fix: copy replacement binaries to `/tmp/` (`tmpfs`, no `nosuid`), set permissions and context
    (`chmod 755`, `chcon <exec_context>`), then `mount --bind /tmp/<bin> /vendor/bin/hw/<bin>`.
  - Starting vendor HALs from shell: `init` rejects `ctl.start$<vendor_svc>` from shell context.
    Inject `allow shell ctl_start_prop property_service set;` via `apd sepolicy --live` before running `start <svc>`.

Safe to restart: HAL binaries (`killall <hal-binary>`, init respawns them if `class hal`), `restart` on a
single HAL's init service, and `surfaceflinger` — which recovers on its own **only if** `hwservicemanager`
is healthy.

**Not safe — never use as a test:**
- `setprop ctl.restart hwservicemanager` — takes down every HIDL client (SurfaceFlinger's HWComposer,
  sensorservice, camera, keymaster, TEE, secure_element, cas) at once. Observed: SurfaceFlinger crash-loops
  on `aidl/SurfaceFlingerAIDL`, zygote and system_server go with it, the display never returns, reboot is
  the only exit. It does re-parse the manifest; the cost is a framework-wide outage.
- `stop; start` (kills the whole `core` class) — same class of outage.
- `restart zygote` — full framework restart; apps lose state and the display blanks.

Recovery paths:
1. **Framework wedged, adb alive** → `adb reboot`. Bind mounts and pushed files vanish; back to the build.
2. **`/vendor` file damaged with no overlay** → `adb reboot recovery`, but `/vendor` is dm-verity there and
   `mount /vendor` fails ("not in fstab"), so a file pushed into `/vendor` from a booted system is not
   reachable: reflashing the vendor image or formatting data are the only options. Treat any direct
   `/vendor` write as capable of costing the user a format.
3. **Screen dead, `adb shell` alive** → check `getprop init.svc.surfaceflinger` and `init.svc.zygote`;
   `restarting` with no `pidof system_server` means reboot, not repair.

### VINTF phantom declarations

- Declaring an `fqname` that no binary registers makes the service **declared but unregistered** (`DM,FC`
  in `lshal`, no PID). Clients resolve it as a dead proxy instead of failing fast — the declaration hides
  a broken HAL rather than fixing a log message.
- A declared-but-absent HIDL service turns a one-line client error into a lazy-start loop, 4 lines/second
  per instance (`Since ... is not registered, trying to start it as a lazy HAL` → `Tried to start ... but
  was unable to` → `init: Control message: Could not find ...` → `libc: PROP_ERROR_HANDLE_CONTROL_MESSAGE`).
- **Confirm the implementation exists before declaring it**: `lshal` registration, `ls /vendor/lib*/hw/`,
  and `strings` on the daemon that should register it.
- **The retry loop belongs to the client.** An APK probing a version that was never shipped retries once
  per second regardless of the manifest. Silence the client or ship the HAL; never paper over it with
  declarations.
- Diff against the stock dump's `vendor/etc/vintf/manifest.xml` before adding or removing any instance, and
  validate host-side with `assemble_vintf` + `checkvintf` (above) before the reboot that tests it.

## Subsystem playbooks

Read the matching section before editing anything in that subsystem.

### Audio

- Never rely on `dumpsys media_session` or app-level `PlaybackState` to conclude audio is playing. Verify
  with `dumpsys media.audio_flinger`: `Standby: yes/no`, active tracks, written frames, underruns.
- logcat main/system wrap fast when verbose HALs flood them (`powerhal-libperfmgr` `sched_setattr`). Check
  the audio HAL PID, `dmesg`, and direct dumpsys before concluding logs are missing.
- Stock MTK audio uses DTS processing (`libdtsaudio.so`, `mtk_bessound`). In AOSP, device effects with
  `EFFECT_FLAG_TYPE_POST_PROC` can be rejected on stream output sessions unless patched or given a software
  fallback.
- Speaker media/game audio disproportionately quiet at mid slider: check `AUDIO_STREAM_MUSIC`
  `DEVICE_CATEGORY_SPEAKER` in `configs/audio/audio_policy_volumes.xml` — AOSP's default curve applies
  −20.8 dB at step 8 (~9% linear amplitude). Use a balanced curve
  (`<point>1,-4500</point> <point>33,-2500</point> <point>66,-1100</point>`) and keep
  `ro.vendor.mtk_besloudness_support=1` in `vendor.prop` for DRC.
- Transsion dual stereo / 2-in-1 speaker & loopback controls: Infinix Hot 11S features dual stereo speakers
  (top earpiece acts as secondary speaker). `Tran_2N1_Speaker_Switch` in kernel is driven by the
  `speaker_2n1_output` path in `audio_device.xml`. `audio.primary.mt6768.so` explicitly searches for
  `speaker_2n1_output`. Transsion loopback is driven by `tran_loopback` and `tran_midtest_loopback_speaker`
  (`Tran_LoopBack_Switch`).
- `proprietary-files.txt` extracts `vendor/etc/audio_device.xml` directly from stock, so
  `configs/audio/audio_device.xml` in this tree must match stock byte-for-byte to prevent duplicate install
  rule collisions or stale RMX2020 divergence.
- Live audio verification recipes:
  1. Playback thread: `dumpsys media.audio_flinger | grep -B 2 -A 35 "Standby: no"`. Verify `Frames written`
     increasing, `rate=1.000000`, 0 underruns, format (`AUDIO_FORMAT_PCM_16_BIT` / `32_BIT`), stereo.
  2. Capture thread: `dumpsys media.audio_flinger | grep -A 25 "Input thread.*RECORD"` (type 3 `RECORD`,
     `AUDIO_SOURCE_MIC`, `Frames read`).
  3. Volume curves: `dumpsys media.audio_policy | grep -A 20 -- "-AUDIO_STREAM_MUSIC (id: 6)"` confirms active
     `DEVICE_CATEGORY_SPEAKER` millibel attenuation points.
  4. Kernel ALSA DAI: `su -c 'dmesg | grep -iE "mt6358-sound|mt6768-audio|afe-pcm"'` confirms sample rate,
     stream direction, and volume ramping.


### Wi-Fi sharing / tethering

- On a 4.14 kernel, `bpf_map_update_elem` on array map key 1 fails with `EINVAL` because array map pre-allocation
  rejects updates if the map is initialized in certain states. `BpfNetMaps.java` in `packages/modules/Connectivity`
  aborts in `initBpfMaps()` when updating stats config map, leaving `sUidOwnerMap == null` while `sConfigurationMap != null`.
  Netd firewall chains then reject all user app traffic (UID >= 10000), breaking all internet connectivity on Wi-Fi and LTE.
  Fixed by `fuck-bpf` patch `packages/modules/Connectivity/0013-BpfNetMaps-do-not-abort-init-when-stats-config-update-fails.patch`.
- On a 4.14 kernel without eBPF TC tethering forwarding, tethered clients connect but get no routing while

  BPF offload is active. Set `<bool name="config_tether_enable_bpf_offload">false</bool>` in
  `overlay/TetheringConfigTarget/res/values/config.xml` to force Netfilter/iptables MASQUERADE; verify with
  `dumpsys tethering` (`enableBpfOffload: false`).
- Hotspot WPA2/WPA3 (`SECURITY_TYPE_WPA3_SAE_TRANSITION`) failures: check
  `dumpsys wifi | grep -E "SupportedFeatures=|config_wifi_softap_sae_supported"` and reproduce from shell
  with `cmd wifi start-softap "TestAP" wpa3_transition "12345678"` / `cmd wifi stop-softap`. If
  `config_wifi_softap_sae_supported` is `false` in `WifiResTarget`, `SoftApCapability` omits
  `SOFTAP_FEATURE_WPA3_SAE` (`SupportedFeatures=43` instead of `47`), Settings still offers the option,
  and `ApConfigUtil` rejects it with `Error, SAE requires HAL support` (`SoftApState mFailureReason=2`).
  `wlan_drv_gen4m.ko` and AIDL `hostapd` support both `wpa3_transition` and `wpa3`.
- DHCP IP but no internet (DNS): check `ps -eZ | grep dnsmasq`, `ss -tulnp | grep -E ":53|:67"`,
  `dumpsys netd | grep -iE "dnsmasq|tetherStart|tetherDns"`. `netd` only launches `dnsmasq` with port-53
  forwarding when `usingLegacyDnsProxy=true` (kept by the `fuck-bpf` patch
  `packages/modules/Connectivity/0022-Tethering-restore-legacy-DNS-proxy-for-dnsmasq.patch`, which reverts
  `0019`). Do **not** set `config_tether_enable_legacy_dhcp_server=true` as a workaround: with
  `usingLegacyDnsProxy=false`, `TetherController` still passes `--port=0` and `dnsmasq`'s `192.168.42–47.x`
  ranges miss `IpServer`'s randomized `ap0` subnet, breaking lease assignment too.

### Bluetooth

- **Controller opcode `0x0C5A` (`READ_DEFAULT_ERRONEOUS_DATA_REPORTING`) crash trap**:
  MediaTek combo chips (MT6768) advertise support for this HCI command in their `local_supported_commands`
  bitmask (byte 18, bits 2 & 3). However, when the Android GD Bluetooth stack queries it during initialization,
  the chip firmware responds with a `Command Status` (`UNKNOWN_HCI_COMMAND`) event instead of `Command Complete`.
  The stack asserts in `packages/modules/Bluetooth/system/gd/hci/hci_layer.cc:278` (`READ_DEFAULT_ERRONEOUS_DATA_REPORTING
  was not expecting status event`) and crashes `com.android.bluetooth` with `SIGABRT` (`tombstone_10`).
- **AIDL HAL implementation requirements (`hardware/mediatek/aidl/bluetooth/BluetoothHci.cpp`)**:
  1. **Mask broken command bits**: In `hciEventReceived`, intercept `CommandComplete` for
     `READ_LOCAL_SUPPORTED_COMMANDS` (`0x1002`, `raw_event[0]==0x0E && raw_event[3]==0x02 && raw_event[4]==0x10`)
     and clear byte 18 bits 2 and 3 (`raw_event[24] &= ~0x0C;`). This prevents the host stack from ever sending `0x0C5A`.
  2. **Status event fallback**: Convert any stray `0x0F` Command Status event for opcode `0x0C5A` into a
     synthetic `0x0E` Command Complete event (`{0x0e, 0x04, num_pkts, 0x5a, 0x0c, 0x01}`) to satisfy the AOSP assert.
  3. **Death recipient linking**: Must call `mDeathRecipient->LinkToDeath(cb)` in `initialize()` and `UnlinkToDeath`
     in `close()`. If omitted, crashes leave the HAL in `ONE_CLIENT` state (`state = 2`). The framework's restart
     attempt then gets `Status::ALREADY_INITIALIZED`, which fatally crashes AOSP in `hci_backend_aidl.cc:40`
     (`initializationComplete: status == AidlStatus::SUCCESS`).
- **Live verification**:
  `svc bluetooth enable` / `svc bluetooth disable`. Check status with `dumpsys bluetooth_manager` (`enabled: true`,
  `state: ON`). Verify active scanning, ACL link (`LST_CONNECTED`), and profile loading (`A2DP`, `HEADSET`, `HID_HOST`, `PAN`).

### USB and ports

- 4.14 uses `/sys/class/dual_role_usb/dual-role-usb20` (`musb-hdrc`), **not** Linux Type-C
  (`/sys/class/typec/`). Do not use AIDL `android.hardware.usb-service.mediatek`: it relies on
  `/sys/class/typec/` and reports 0 ports on this kernel.
- Use the prebuilt `android.hardware.usb@1.1-service-mediatek`. MTP needs it: AOSP's `MtpServer` is
  FunctionFS-based and opens `/dev/usb-ffs/mtp/ep0`, but this kernel has no `CONFIG_USB_F_MTP`, so configfs
  `mtp.gs0` is a stub and `usb_get_function name=mtp` never resolves. MTK implements MTP through the legacy
  `android_usb` gadget (`/dev/mtp_usb`), which only the vendor HAL drives. With `dual_role_usb`,
  `MtpService` logs `Couldn't get control FD!`.
- That prebuilt installs no VINTF fragment of its own: declare `@1.1::IUsb/default` and
  `@1.1::IUsbGadget/default` in `manifest.xml`, and keep `android.hardware.usb.gadget@1.0`, `@1.1`,
  `android.hardware.usb@1.0`, `@1.1` in `device.mk` (the gadget libs are otherwise only pulled in by other
  services).
- `device/mediatek/sepolicy_vndr` `base/vendor/file_contexts` already labels
  `android\.hardware\.usb@1\.[0-9]+-service-mediatek` as `mtk_hal_usb_exec`.

### Telephony and RIL

- `librilfusion.so` in `mtkfusionrild` registers `vendor.mediatek.hardware.mtkradioex` at **`@2.2`** only.
  Only `@2.0`/`@2.1`/`@2.2` stubs exist in `/vendor/lib64`; there is no `@3.0` on this device.
- Declare `@2.2::IMtkRadioEx/*` in `configs/manifests/manifest.xml`, matching stock. Declaring `@2.2` as
  `@2.0` gives `Service vendor.mediatek.hardware.mtkradioex@2.2::IMtkRadioEx/imsSlot1 must be in VINTF
  manifest in order to register/get`, and clients then fall back to lazy starts.
- Do **not** declare `@3.0::IMtkRadioEx/*`. Every client `getService` becomes a lazy-start attempt:
  `Since ...@3.0::IMtkRadioEx/imsSlot1 is not registered, trying to start it as a lazy HAL` +
  `Tried to start ... but was unable to` + `init: Control message: Could not find ...` +
  `libc: PROP_ERROR_HANDLE_CONTROL_MESSAGE (0x20)` — four lines/second/instance, all `DM,FC` in `lshal`.
- The requester is the client, not the manifest: `ImsService.apk` v13 probes `@3.0::IMtkRadioEx` and the
  AIDL `IMtkRadioExIms`/`Voice`/`Modem` slots once per second regardless of what is declared. Fix the
  client path (disable the probing component, or ship the AIDL HAL); never add phantom declarations.

### Battery and charging

- Transsion/MTK `fuelgauged` reports scaled-down ticks in `charge_full_design` (e.g. `231000` instead of
  `5000000`). The true design capacity is `/sys/devices/platform/tran_battery/BATTERY_QMAX` (`5000`).
- Label that node `u:object_r:sysfs_batteryinfo:s0` in `sepolicy/vendor/file_contexts` so
  `hal_health_default` can read it, and in `hardware/mediatek/aidl/health/Health.cpp` override
  `UpdateHealthInfo()` to read it and set `batteryFullChargeDesignCapacityUah = 5000000`.

### Offline charging (`kpoc_charger`)

- `kpoc_charger` is a **32-bit ARM** executable (`system/bin/kpoc_charger`) requiring 32-bit
  `system/lib/libshowlogo.so`. Extract both and set `compile_multilib: "32"` for `libshim_showlogo` in
  `libshims/Android.bp`. Add `PRODUCT_PACKAGES += libsuspend`.
- `init.mt6768.rc`'s `on charger` must start **`vendor.light-default`** (the AIDL light HAL name, not
  `vendor.light`) and chown the LED brightness nodes.
- Remove any `service vendor.charger /system/bin/charger` from `init.mt6768.rc` — `kpoc_charger.rc`
  already defines `service kpoc_charger` in `class charger`.
- 32-bit `libgui`/`libui` shim pitfalls:
  1. Returning `sp<T>` from `extern "C"` trips `-Wreturn-type-c-linkage` under `-Werror`. Under ARM32
     AAPCS, `sp<T>` returns via an sret pointer in `r0`: use placement `new (out) sp<T>(...)` with
     `sp<T>* out` first.
  2. `SurfaceComposerClient.h` has `using gui::LayerMetadata;` inside `namespace android` — do not declare
     `struct LayerMetadata` in the same TU; the `android::LayerMetadata` ctor/vtable symbols live in
     `libshim_showlogo_metadata.cpp`.
  3. In `getActiveDisplayConfig`, Android 15 `ui::DisplayMode` has `id` at `+0` and `std::vector` at `+20`.
     Never cast `DisplayConfig*` to `ui::DisplayMode*` (stack corruption, 4-byte offset shift); copy
     `resolution`, `dpi`, `refreshRate` explicitly.

### Sensors

- Magnetometer is MEMSIC `mmc5603_x6812`, accelerometer Silan `sc7a20`. There is no discrete gyro chip;
  MediaTek implements the virtual gyroscope and sensor fusion (Orientation, Game/Geomagnetic Rotation
  Vector, Rotation Vector, Gravity, Linear Acceleration) inside `sensors.mt6768.so` (`VendorInterface`).
- At startup it reads `/sys/class/sensor/m_mag_misc/maglibinfo` and `dlopen("libmmc5603_x6812.so", 2)`. If
  `vendor/lib{,64}/libmmc5603_x6812.so` is missing from `proprietary-files.txt`, `VendorInterface` logs
  `get lib_interface fail dlopen operation`, and Magnetometer, Gyroscope, Orientation and all six fusion
  sensors return all zeros in every app. Keep both binaries in `proprietary-files.txt` under `# Sensors`.
- **MEMSIC current compensation denial**: The compass driver attempts to open `/sys/class/power_supply/battery/current_now`
  and `/sys/class/power_supply/usb/online` to compensate magnetic distortion caused by charging current. Requires
  `allow hal_sensors_default sysfs_batteryinfo:file r_file_perms;` in `sepolicy/vendor/hal_sensors_default.te`,
  otherwise logcat floods with `memsic_log open SYS_FS_CURRENT failed, Error 13`.
- **Client registration**: Sensors show 0 active clients when auto-rotate is toggled off (`settings get system accelerometer_rotation` = 0).
  Enable with `settings put system accelerometer_rotation 1` to start window orientation listening. Verify active streams with
  `dumpsys sensorservice`.

### Display and refresh rate

- 90 Hz toggle in Settings → Display: `<bool name="config_show_smooth_display">true</bool>` in
  `overlay/SettingsResTarget/res/values/config.xml`; verify with
  `cmd overlay dump com.android.settings.target`.
- Peak and minimum refresh rate selectors (60 Hz / 90 Hz options):
  `<bool name="config_show_peak_refresh_rate_switch">true</bool>` and
  `<bool name="config_show_min_refresh_rate_switch">true</bool>` in `overlay/SettingsResTarget/res/values/config.xml`.
- Keeping 90 Hz active without `min_refresh_rate=90` needs BOTH:
  1. `<integer name="config_defaultRefreshRate">0</integer>` in
     `overlay/FrameworksResTarget/res/values/config.xml` — AOSP's `60` makes
     `PRIORITY_DEFAULT_RENDER_FRAME_RATE` vote `RenderVote(0, 60)` and caps `primaryRanges` to 60 Hz.
  2. Do **not** set `ro.surface_flinger.enable_frame_rate_override=false` in `configs/props/vendor.prop`:
     `VoteSummary`/`RefreshRateSelector` then discard physical modes outside the `render` range and
     `Scheduler` sets `defaultTouchTimerValue=0`.
- Live check: `dumpsys display | grep -E "mBaseDisplayInfo|renderFrameRate|PRIORITY_USER_SETTING"`
  and `dumpsys SurfaceFlinger | grep -A 5 -i "refresh-rate"`.
  Force with `settings put system min_refresh_rate 90`, revert with `settings delete system min_refresh_rate`.
- Cutout and status bar: compare `config_mainBuiltInDisplayCutout` and `status_bar_height_portrait`
  (`FrameworksResTarget`) against stock RRO values, and check `rounded_corner_content_padding`,
  `status_bar_padding_start`, `status_bar_padding_top` in `SystemUIResTarget` for clipped clock/icons.

### Touch gestures / DT2W

- Inspect driver wake keys in the kernel (`Image.gz` disassembly of `*_ts_work_func`). MTK panels (Novatek,
  Focaltech) often emit key 183 (`KEY_F13`) or 143 (`KEY_WAKEUP`) to `/dev/input/event*` while suspended.
- Without a custom keylayout (`usr/keylayout/mtk-tpd.kl`) Android falls back to `Generic.kl`, where 183 maps
  to `F13`, not marked `WAKE` in `PhoneWindowManager`, so the wake event is discarded.
- Verify `powerhint.json`: `DoubleTapToWakeEnable` uses `/proc/gesture_function`, requiring `"cc1"` to
  enable and `"cc2"` to disable.

### UI jank and power

- Jank: measure both 60 Hz and 90 Hz with `dumpsys SurfaceFlinger` (HWC/GPU missed frames, phase offsets),
  `dumpsys gfxinfo <pkg> framestats` (UI/RenderThread/GPU), FrameTracker CUJ logs, and scheduling groups
  (`top-app` vs `foreground`).
- Power: PowerHAL node fds and hint stats (`dumpsys android.hardware.power.IPower/default`), CPU/GPU clock
  scaling (`/sys/devices/system/cpu/cpufreq/`, `/sys/kernel/ged/hal/`), EAS/stune (`/dev/stune/`). Watch for
  kernel ABI mismatches, e.g. a 4.14 kernel rejecting newer uclamp `sched_setattr` with `E2BIG`/`err=7`.
- Benchmark with a deterministic sequence, never gestures: `input swipe` opens or closes the shade
  depending on current state, varying frame counts 4× between identical runs.

```bash
cmd statusbar collapse; sleep 0.5
dumpsys gfxinfo com.android.systemui reset >/dev/null
i=0; while [ $i -lt 4 ]; do
  cmd statusbar expand-notifications; sleep 0.9
  cmd statusbar collapse; sleep 0.9; i=$((i+1))
done
dumpsys gfxinfo com.android.systemui | grep -E "Total frames|Janky frames:|percentile"
```

- "Achieved frame rate" needs `Total frames rendered` ÷ wall time. Percentiles alone cannot distinguish
  40 from 90 fps, because a consistently late surface gets a multi-vsync deadline and is reported as *not*
  janky.
- Cold app launch is only ~25 frames, so one late frame reads as ~4% jank; compare scroll/workload numbers
  against the tuned baseline (Settings scroll 0.19%, Launcher swipe 0.21%, Dialer scroll 0.52%, Jelly 1.91%).
- Measurement invalidators: `debug.hwui.renderer` set with `setprop` sticks for every process started
  afterwards (confirm with `dumpsys gfxinfo <pkg> | grep -m1 Pipeline`; shipped default is OpenGL,
  `ro.hwui.use_vulkan` unset/empty). Every dump line must come from one process generation — reset *after*
  a SystemUI restart, wait ~8 s, wake the device (`input keyevent KEYCODE_WAKEUP; wm dismiss-keyguard`), and
  check the first run is not `Total frames rendered: 0`. `killall com.android.systemui` can fail under
  APatch `su`; use `kill -9 <pid>` from `ps`. Killing the power HAL leaves `PowerManagerService` on a dead
  binder for ~2 s; verify with `dumpsys android.hardware.power.IPower/default` that `Current Index` matches.
- MTK boost nodes (`configs/powerhint.json`, runs on `hal_power_default`): `scaling_min_freq` is
  **ignored** (PPM owns the floor — a LAUNCH hint for 1800000/2000000 leaves it at 500000/850000);
  `scaling_max_freq` only caps. The sanctioned path is
  `/proc/perfmgr/boost_ctrl/eas_ctrl/perfserv_{ta,fg,bg}_boost` (−100..100) and
  `perfserv_{,ta_,fg_,bg_}uclamp_min` (0..100), the same set stock uses in `vendor/etc/powercontable.xml`.
  `/proc/perfmgr/boost_ctrl/cpu_ctrl/perfserv_freq` (`"<c0min> <c0max> <c1min> <c1max>"`, `-1` = leave) is
  the only node that moves the reported min frequency, and stock does not use it.
  `/dev/stune/*/schedtune.prefer_idle` cannot be written by the power HAL (only `schedtune.boost` is
  granted via the `cgroup` label); the MTK equivalent is `perfserv_prefer_idle`, a multi-line per-cgroup
  array. Every `ResetOnInit` node needs an explicit `DefaultIndex`, otherwise libperfmgr logs
  `Failed to read Node[N]'s DefaultIndex, set to last index` and resets the node to its **last** value at
  HAL start (e.g. a permanent 10% `top-app` boost).
- Schedutil and scheduler latency: `/system/etc/init/hw/init.rc` writes `sched_latency_ns 10000000` (10 ms)
  during `on init`, overwriting earlier `early-init` vendor configurations. Enforce `sched_latency_ns=4000000`
  (4 ms), `sched_min_granularity_ns=500000` (500 µs), `sched_wakeup_granularity_ns=2000000` (2 ms),
  `sched_migration_cost_ns=200000` (200 µs), `up_rate_limit_us=500`, and `down_rate_limit_us=5000` in
  `init.mt6768.power.rc` under `on property:sys.boot_completed=1`. The 1 ms default down rate decays clocks
  between consecutive frame presentations; 5 ms holds them across VSYNC intervals.
- Mali DVFS and GPU intervals: `/sys/kernel/ged/hal/dvfs_margin_value=50`,
  `/sys/kernel/ged/hal/timer_base_dvfs_margin=798`, `/sys/module/ged/parameters/gx_force_cpu_boost=1`,
  `/sys/devices/platform/13040000.mali/dvfs_period=50`. Stock clamped the margin to 10 on `sys.boot_completed=1`,
  starving the Mali-G52 at 299 MHz during 3D and window transition loads. In `configs/powerhint.json`,
  `GEDMargin` and `GPUDVFSInterval` must both declare `DefaultIndex: 2` (50 and 50 ms), and `EASMigrateCost`
  must declare `DefaultIndex: 2` (200 µs); otherwise libperfmgr's `ResetOnInit` actively resets margin to 10
  and period to 100 ms on every HAL boot.
- `configs/powerhint.json` profiles: `LAUNCH` sets `DRAMOppMin=0` (max memory bandwidth), `CCIFreqMode=1`,
  `GEDMargin=50`, `GPUDVFSInterval=50`, `EASMigrateCost=200000`, `PerfservFreq="1800000 -1 2000000 -1"`,
  and `TASchedtuneBoost=40` + `perfserv_ta_boost=100` / `perfserv_fg_boost=100` to migrate top-app threads
  to the A75 cluster during window creation. `INTERACTION` keeps `GEDMargin=50` and `EASMigrateCost=200000`.
- App Freezer on 4.14: Kernel 4.14 lacks cgroup v2 freezer (`/sys/fs/cgroup/cgroup.freeze` does not exist).
  If an app spins in freezer binder callback retry loops (`ActivityThread: Too many transaction errors, throttling freezer binder callback`),
  check with `dumpsys activity freezer` and `ps -A`.
- Boot DEX2OAT in `configs/props/system.prop`: `dalvik.vm.boot-dex2oat-cpu-set=0,1,2,3,4,5,6,7` and
  `dalvik.vm.boot-dex2oat-threads=8`. Without them ART's `odrefresh`/`artd` fall back to
  `0,1,2,3,4,5` and exclude both 2.0 GHz A75 cores during boot compilation.
- `/sys/block/mmcblk0/queue/nr_requests=64` on boot completion shaves ~7 ms off cold starts. `/sys/block/sdc`
  (UFS) does not exist on this SoC.
- `pm.dexopt.install=speed-profile` and `pm.dexopt.bg-dexopt=speed-profile` are what the tree ships. On fresh
  first boots, apps remain uncompiled in `[status=verify] [reason=first-boot]`, forcing the ART interpreter
  to execute all bytecode and artificially skewing cold launch jank to 20–30%. Check
  `dumpsys package <pkg> | grep 'arm64:'` before benchmarking; compile test targets with
  `cmd package compile -m speed-profile -f <pkg>`.
- SurfaceFlinger phase offsets in `configs/props/vendor.prop`, where
  `sfWorkDuration = vsyncDuration − sfOffset` and `appWorkDuration = vsyncDuration + (sfOffset − appOffset)`.
  90 Hz: `debug.sf.high_fps_late_sf_phase_offset_ns=0`, `high_fps_late_app_phase_offset_ns=-14000000`,
  `high_fps_early_phase_offset_ns=0`, `high_fps_early_app_phase_offset_ns=-14000000`,
  `high_fps_early_gl_phase_offset_ns=0`, `high_fps_early_gl_app_phase_offset_ns=-14000000`,
  `ro.surface_flinger.max_frame_buffer_acquired_buffers=4`. 60 Hz:
  `ro.surface_flinger.vsync_sf_event_phase_offset_ns=0`,
  `ro.surface_flinger.vsync_event_phase_offset_ns=-4000000`. Keep
  `debug.sf.enable_gl_backpressure=0` — AOSP's `true` stalls the GPU queue on Mali-G52 and spikes launcher
  jank to 23%. Keep `ro.surface_flinger.use_content_detection_for_refresh_rate=false` to prevent dynamic
  60Hz/90Hz mode flips and phase resync drops during rapid user interaction. `debug.sf.hwc.min.duration`
  stays 5 ms (3 vs 5 is a wash). Interactive scroll easily sustains <2% jank across all standard apps
  (Settings Homepage 1.94%, QuickstepLauncher 0.37%, Contacts 0.57%, Clock 0.00%, Dialer 0.57%, Calculator 0.57%).


### Camera and media (TranssionCamera / Codec2)

- Separate app crashes from HAL/kernel failures: clear tombstones and logs, exercise one mode at a time
  (`AI CAM`, `VIDEO`, `BEAUTY`, `PORTRAIT`, `SUPER NIGHT`, `DOCUMENTS`), then read `logcat -b crash -d`,
  `/data/tombstones/`, `dmesg | grep -E "avc: denied|MTK_V4L2|MTK_VCODEC"`. logcat floods at 30+ fps with
  `TranCam/Plugin_TranStream` and `Hal3ARaw` — always query `logcat -b main,crash -d -t 500 '*:W'` or
  `-s <tags>`, and look in `/data/tombstones/` for `camerahalserver` (`MF@CapPipe`) crashes.
- Isolate Codec2/VPUD from `StagefrightRecorder` with `screenrecord --time-limit 2 /data/local/tmp/sr.mp4`
  while watching `logcat -b main -d -s VPUD:* C2MtkVenc:* MPEG4Writer:* MediaRecorder:*`.
- Test APK smali/dex live by signing with `testkey` and bind-mounting over
  `/system_ext/app/TranssionCamera/TranssionCamera.apk`, then `stop && start`. Test vendor `.so` changes
  with `adb push`, `chcon u:object_r:vendor_file:s0`, and `stop camerahalserver && start camerahalserver`
  (or `killall vpud`).
- Green screen in screen recording: VirtualDisplay buffers (`GraphicBufferSource`, usage `0x10300`) allocate
  as AFBC when `vpu_runtime_caps` is 0 in
  `android.hardware.graphics.{allocator,mapper}@4.0-impl-mediatek.so`; `libdpframework.so` cannot decode
  AFBC in `DpEngine_RDMA` (`config frame error -9`), leaving YV12 frames zeroed.
- Green line/shadowing in recordings: odd multiples of 16 width (e.g. 1072×2448) make gralloc4 align chroma
  stride to 16 bytes (`uvPitch=544`) while `MTK_V4L2` venc expects `yPitch/2=536`. Keep
  `<Limit name="alignment" value="32x16" />` in `media_codecs_c2.xml` so width aligns to 32.
- Screen recording crash on start (`Cannot find throughput setting`): SystemUI defaults to HEVC when
  `c2.mtk.hevc.encoder` is declared, and `libvcodecdrv.so` (`HEVC_ThroughputSetting`) has no profile for
  90 fps and rejects >1080p at any framerate other than 30. Do not declare `c2.mtk.hevc.encoder` (stock
  disabled the C2 HEVC encoder); keep `config_screenRecorderMaxFramerate` ≤ 60 in `SystemUIResTarget`; add
  `<Feature name="can-swap-width-height"/>` to `c2.mtk.avc.decoder` and `encoder` so portrait 1080×2460
  is not downscaled to 948×2160.
- Playback failure (`VIDIOC_QBUF Failed: Invalid argument` / `dma_general_buf is err`): the Xiaomi-derived
  `libcodec2_mtk_vdec.so` cannot queue DMABUFs to Infinix's `vcodec_dec` driver
  (`vb2ops_vdec_buf_prepare`). Do not declare `c2.mtk.*.decoder` video decoders; patch
  `libcodec2_mtk_c2store.so`'s static loader table via `extract-files.py` (`sig_replace`) so Google's NEON
  `c2.android.*.decoder` software decoders handle video.
- Audit both `System.loadLibrary` (`dexdump`) and `dlopen` strings (`strings` for `\.so$` and `/vendor/`)
  against `proprietary-files.txt`: `TranssionCamera` and `libmtkcam_3rdparty.customer.so` load
  `libanc_hdr.so` (`vendor/etc/anc_siq/*`), `libcameracustom.plugin.so` (`libremosaic*`, `librmsclib1.so`)
  and `libcommon.so` (`libXDocProcessSDK.so`) without `DT_NEEDED`.
- Stock `libsdk_sr.so` has a 4-byte NEON tail over-read (`0x1c8c50: ldr d16, [x8]`) — harmless on Android
  11 jemalloc, but it hits Scudo's guard page on Android 15+; fixed via `sig_replace` in `extract-files.py`.
- Hybrid `vpud` (stock Android 11 `libvpud_vcodec.so`) + `libcodec2_mtk_venc.so` breaks `MediaRecorder` even
  when `screenrecord` and `Aperture` succeed: `AP_IPIMSG_ENC_SET_PARAM 15` must return `0`
  (`0x12a2c: movs r7, #0`) and `u4TSVC` (`0x11b42`) must be zeroed so `VPUD` does not inject a 9-byte
  `PrefixNALU` that `MPEG4Writer::parseAVCCodecSpecificData` rejects.
- Recording container validation via `ffprobe`: After `screenrecord --verbose --time-limit 5 /data/local/tmp/test.mp4`,
  verify container parameters on host: `ffprobe /tmp/test.mp4` must report progressive `h264 (High) (avc1)`, exact
  native resolution (`1080x2460`, confirming `can-swap-width-height` prevented downscaling), expected bitrate
  (~16–20 Mbps), and valid audio/video stream indices without timestamps gaps.
- Video playback validation: Monitor `logcat -d | grep AidlBufferPool` during ExoPlayer/Glimpse video playback to
  ensure bufferpool2 recycles buffers (`recycle/alloc` ratios match, 0 dropped frames, no `FatalError(DecodeError)`).
  Check decoder size ranges and capabilities with `dumpsys media.player | grep -A 40 "Decoder \"c2.android.avc.decoder\""`.


## Dead-config sweeps

Before removing a `.rc` line, sepolicy path or blob:

1. **Diff against stock first.** Anything identical to stock is not our bug; deleting it only adds
   divergence and noisier future diffs. Only paths introduced *by this tree* are suspects.
2. **Test every path on the device**, respecting the fstype (`genfscon sysfs` → `/sys`, `genfscon proc` →
   `/proc`): `for p in /sys/... /proc/...; do [ -e "$p" ] || echo "MISS $p"; done`
3. **`ueventd` rules for absent partitions are not dead** — ueventd skips them (`md1dsp`, `secro`, `misc2`).
4. **`disabled` services are not dead** — check for `start X` triggers first. `stp_dump` was only
   removable after confirming `persist.vendor.connsys.coredump.mode` is 0.
5. **Stock vendor `.rc` files list services whose binaries we do not extract** (`bt_dump`, `wifi_dump` in
   `init_connectivity.rc`). init tolerates this; do not patch the rc.

## Essential commands

```bash
# Regenerate vendor tree after ANY proprietary-files.txt change
# (needs user permission; must finish before the build starts)
./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30

# Refresh pinned-blob sha1s (must leave `git diff` clean on second run)
python3 update-sha1sums.py
```

## Commit style

Structure: `<codename>: <subsystem>: <summary>` (e.g. `X6812B: wifi: enable WPA3-SAE softap support`).
1. Subject line: imperative, concise.
2. Log snippet: verbatim error, logcat trace, or failing command output showing the bug/repro.
3. Explanation: root cause, what was verified on hardware versus assumed, and why the chosen fix is correct.
No explanatory comments in code/config files; put all reasoning in the commit message.
