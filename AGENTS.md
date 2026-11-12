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
2. **Never run `grep`/`rg` at `$ANDROID_ROOT` or on `out/`.** Scope every search
   to a specific directory:
   ```bash
   # correct
   grep -rnF "android.hardware.biometrics.fingerprint@2.1" $ANDROID_ROOT/hardware/interfaces/
   # forbidden
   grep -rn "foo" $ANDROID_ROOT
   ```
3. **Never edit AOSP/Lineage source or `vendor/infinix/X6812B`.** Every fix must
   come from this device tree. The vendor tree is generated output only.
4. **Never `git reset --hard`.** It has already destroyed an uncommitted user
   edit in this tree. Use `git checkout -- <path>` on only the files you touched,
   then `git status --short` to confirm you are not sweeping up user work.
5. **No explanatory comments in files.** Put the reasoning in the commit message.
6. **Before adding an `.rc` service, prop, sepolicy rule, or blob, check whether
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

- Prefix device commits with `X6812B: `.
- Put all reasoning in the commit message: what the symptom was, the exact error
  text, what you verified, and why the chosen fix is correct. Do not add
  explanatory comments to the files themselves.
- State what you verified versus what you assumed. If a claim rests on an
  observation, include the command or output that proved it.

## Maintaining this file

Update this file whenever you fix something non-obvious: how to reproduce, how
to diagnose, what you tried that failed, and any mistake you made. Prefer adding
to an existing section over creating a new one. Keep entries actionable — a
command or a concrete check beats a description.
