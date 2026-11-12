- you are an aosp or lineage os expert. your job is to bring up device tree, fix error, debug, read source code, etc
- lineage os root dir or $ANDROID_ROOT is on ../../../
- you are not allowed to run grep or rg on the lineage os root path and build out dir. instead, do targeted search like: grep -rnF "android.hardware.biometrics.fingerprint@2.1" $ANDROID_ROOT/hardware/interfaces/
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

## Fast sepolicy validation (no full build)

`checkpolicy` is a standalone host binary, so selinux can be validated in ~1 minute
instead of a full build. The only input soong generates is `vendor_sepolicy.conf`,
which is a plain `m4` invocation. Extract it from the ninja once:

```
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
```

Then the loop is:

```
bash /tmp/opencode/run.sh          # regenerate vendor_sepolicy.conf
out/host/linux-x86/bin/checkpolicy -C -M -c 30 \
  -o /tmp/out.cil.raw out/soong/.intermediates/system/sepolicy/vendor_sepolicy.conf/android_common/X6812B/vendor_sepolicy.conf
```

Repeat the same extraction for `# Module:  sepolicy_neverallows.checkpolicy.conf`
and run `checkpolicy -M` on it -- that is what catches `neverallow` violations
(unknown types alone will not).

Gotchas:
- `checkpolicy` prints `ERROR 'unknown type X'` but still exits 0. Always read the
  output, do not trust the exit code, and confirm the output file was created.
- the `.conf` files are soong-generated and go stale. After editing any `.te`
  you MUST re-run the `m4` step or you are validating the old policy.
- extract the `m4` command from `out/soong/build.lineage_X6812B.9.ninja`
  (the split ninja files, not the single `build.lineage_X6812B.ninja`).

## selinux rules that bite

- `init_daemon_domain(X)` expands to `domain_auto_trans(init, X_exec, X)`, so it
  references a type named `X_exec`. Never delete a `*_exec` type just because a
  literal grep finds no other user -- grep for the macro argument instead.
  Same trap for `add_hwservice(X, Y)`, `binder_use(X)`, `r_dir_file(X, Y)`.
- an "unused type" scan based on literal text matching is unsound. Prefer letting
  `checkpolicy` prove it.
- vendor policy cannot reference types declared in
  `device/mediatek/sepolicy_vndr/base/private/` (that is platform policy). e.g.
  `proc_cl_cam_status` lives there, so a vendor `.te` referencing it fails with
  `unknown type`.
- `vendor_default_prop` is under `treble_sysprop_neverallow`
  (`system/sepolicy/private/property.te:158`). Granting it to a vendor domain
  (e.g. `cameraserver`) compiles but fails the neverallow check.
- versioned app domains are siblings, not children: `untrusted_app` and
  `untrusted_app_29` are separate types, so `get_prop(untrusted_app, ...)` does
  NOT cover an app with `targetSdkVersion=29`. Use `untrusted_app_all`, which
  `untrusted_app_domain()` populates for every version.
- when adding a `genfscon`/`file_contexts` path, test it on the running device
  first (`ls -Z <path>`). A wrong path silently never matches -- e.g. our
  `OTG_CTL` entry pointed at the mt6781 `odm/odm:tran_battery` path so the node
  stayed labelled generic `sysfs`, and system_server's grant could never apply.
- the shared `device/mediatek/sepolicy_vndr` already covers most MediaTek nodes
  (charger, batteryinfo, wakeup, extcon, vibrator, block queues). Check there
  before adding an entry; duplicated/bogus entries from another device tree are
  the usual cause of dead policy in this tree.

## build error triage

- `out/error.log` is overwritten by each build attempt; always check its mtime
  against file mtimes before trusting it. A `libmtknetutils.so does not exist`
  error was purely a race: the build started 42s before
  `./extract-files.py` finished writing 629 blobs. Re-run extract to completion,
  then build.
- two soong stages fail differently and it matters which:
  - "analyzing Android.bp files and generating ninja" = soong/blueprint level
    (missing src file, AIDL version clash, bad module).
  - "writing legacy Make module rules" / `overriding commands for target` =
    two modules install to the same destination. Grep
    `out/soong/installs-lineage_X6812B.mk` for the target: it lists every rule
    and you can see both sources.
- `stem:` changes only the INSTALLED filename, not the module name. Two modules
  can therefore produce one destination. A prebuilt blob whose `DT_NEEDED` is
  baked in (e.g. `libui-v32.so` wanting
  `android.hardware.graphics.common-V2-ndk_platform.so`) needs the installed
  name to match, but check whether AOSP/Lineage already installs that name
  (`hardware/lineage/compat`) before adding a `stem`.
- never point a module at a source-built AIDL stub when a pure prebuilt exists:
  compat's `*-ndk_platform` pulls `...-V2-ndk-source` while
  `libui -> libgralloctypes` pulls `...-V6-ndk-source`, and soong rejects the
  variant with "depends on multiple versions of the same aidl_interface".
  Pure `vndk_prebuilt_shared`/`cc_prebuilt_library_shared` avoid this.
- an output file existing from an older build (e.g. `build.lineage_X6812B.ninja`
  from a run that succeeded before a later failure) does not mean the current
  config builds. Check timestamps against the failing log.

## blob / proprietary-files.txt conventions

- pinning is driven by the `|hash` suffix, not by the `# ... - from` comment.
  `extract_utils` treats a file with `file.hash is not None` as pinned and
  restores it from the vendor tree instead of re-extracting.
- a pinned file WITH a blob fixup needs TWO hashes: `source|fixed-up`. A single
  hash holding the post-fixup value always mismatches, so the pin silently never
  applies and the log says `fixed up` instead of `restored pinned file ...`.
- do not put the literal substring ` - from` in a section header unless you
  intend `update-sha1sums.py` to hash everything beneath it -- it keys off that
  exact text and will also rewrite/collapse `source|fixup` pairs to one hash.
- `update-sha1sums.py` should be idempotent: run it twice and confirm `git diff`
  is empty the second time.
- no real error, just noise: `restored pinned file with hash X has fixup hash X`
  printed in RED is a request to add the second hash, not a failure -- it
  returns `MATCH`. Compare with `BAD_FIXUP` which is a genuine error.
- header comments must be accurate. This tree's file had AEE/Widevine/Graphics
  Hardware/WiFi attributed to Redmi/realme firmware, but every one of those
  blobs exists in the stock X6812B dump and only differs by the fixups in
  `extract-files.py`. Only the Media (Codec2) section is external
  (`mt6768-dev/android_device_xiaomi_mt6768-common`).

## dead-config sweep discipline

When sweeping `.rc` / `sepolicy` for dead entries (this tree inherited whole
files from the mt6781 X670 tree in `9529c41 "replace sepolicy with X670 sepolic"`):

- diff each file against the stock dump first. Anything identical to stock is
  not your bug -- deleting it just creates divergence and noisier future diffs.
  Only paths introduced BY this tree are suspects.
- test every path on the running device before removing:
  `for p in ...; do [ -e "$p" ] || echo "MISS $p"; done`
- respect the fstype when building the test path: `genfscon sysfs` -> `/sys`,
  `genfscon proc` -> `/proc`.
- do not assume "absent" means dead for ueventd rules -- `ueventd` skips rules
  for partitions that do not exist (`md1dsp`, `secro`, `misc2`).
- `.rc` files shipped by the stock vendor image (e.g. `init_connectivity.rc`)
  still contain services whose binaries are not extracted (`bt_dump`,
  `wifi_dump`). init tolerates that; no need to patch the rc.
- services declared `disabled` are not dead -- check for `start X` triggers
  before removing (that is why `stp_dump` was only removable after confirming
  `persist.vendor.connsys.coredump.mode` is 0).

## device-tree safety

- NEVER use `git reset --hard` in this tree; it destroys uncommitted user edits
  (it wiped a manual `AGENTS.md` change). Use `git checkout -- <specific path>`
  on only the files you touched.
- before committing, `git status --short` and confirm you are not sweeping up
  files the user edited.

