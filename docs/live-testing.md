# Live device testing

The device is the user's daily driver. Live testing is how this tree gets verified, but every write has a
revert path that must be known *before* the write, not worked out afterwards.

## Ground rules

- **Back out first.** `adb pull` the current file into the workspace and record its sha256. The revert must
  be a one-liner.
- **Never overwrite a working system file wholesale.** Change the specific line (`sed -i '/pattern/d'`,
  targeted edit). A whole-file push that is well-formed but wrong costs a data format.
- **Prefer `/data/local/tmp` + `mount --bind`** over writing into `/vendor` for anything bind-mountable
  (manifest, props, JSON configs, effect XMLs). The mount lives in memory: reboot discards it, `umount`
  is instant, and it cannot brick boot.
- **Verify on the device, not on a host copy.** `adb shell grep <new-string> <path>`. A host-side pull can
  be stale; the parse happens in-guest.

## Parse-at-boot semantics

| File | Parsed by | Live edit effective after |
| --- | --- | --- |
| `/vendor/etc/vintf/manifest.xml` | `hwservicemanager` (and `libvintf` clients) | reboot, or `hwservicemanager` restart (see warning) |
| `/vendor/etc/vintf/compatibility_matrix.xml` | `checkvintf` at build, framework at boot | reboot |
| `*.rc` under `/vendor/etc/init`, `/system/etc/init` | `init` | reboot (`restart <svc>` reuses the parsed definition) |
| `.prop` files, `build.prop` | `init` property service at first boot | reboot, or `resetprop` + restart the consumer |
| Overlay APKs, `audio_effects.xml`, `powerhint.json` | the owning service | restart that service |

`adb root` + `adb remount` reports "Now reboot your device for settings to take effect": overlayfs merges
the new content into `/vendor` on the **next** boot, not immediately.

## Restarting services: what is safe

Safe — the service owns nothing else, and its clients retry:

- HAL binaries: `killall <hal-binary>` (init respawns them if `class hal`).
- `restart` on a single HAL's `init` service.
- `surfaceflinger` — recovers on its own **only if** `hwservicemanager` is healthy.

**Not safe — never use as a test**:

- `setprop ctl.restart hwservicemanager` — takes down every HIDL client (SurfaceFlinger's HWComposer,
  sensorservice, camera, keymaster, TEE, secure_element, cas) at once. Observed result: SurfaceFlinger
  crash-loops on `aidl/SurfaceFlingerAIDL`, zygote and system_server go with it, display never returns,
  reboot is the only exit. It *does* re-parse the manifest, but the cost is a framework-wide outage.
- `stop; start` (kills the whole `core` class) — same class of outage.
- `restart zygote` — full framework restart; apps lose state and the display blanks during it.

## Recovery paths

1. **Framework wedged, adb alive** → `adb reboot`. Bind mounts and pushed files vanish; back to the build.
2. **`/vendor` file damaged with no overlay** → `adb reboot recovery`. Note that `/vendor` is dm-verity
   there: `mount /vendor` fails ("not in fstab"), so a file pushed into `/vendor` from a booted system is
   not reachable from recovery. The only options are reflashing the vendor image or formatting data.
   Treat any direct `/vendor` write as capable of costing the user a format.
3. **Screen dead, `adb shell` responding but no display** → check `getprop init.svc.surfaceflinger`,
   `getprop init.svc.zygote`; `restarting` with no `pidof system_server` means reboot, not repair.

## VINTF manifest specifics

- Declaring an `fqname` that no binary registers makes the service **declared but unregistered**
  (`DM,FC` in `lshal`, no PID). Clients resolve it as a dead proxy instead of failing fast — a phantom
  declaration hides a broken HAL rather than fixing a log message.
- A declared-but-absent HIDL service turns a one-line client error into a lazy-start loop, 4 lines/second
  per instance (`Since ... is not registered, trying to start it as a lazy HAL` → `Tried to start ... but
  was unable to` → `init: Control message: Could not find ...` → `libc: PROP_ERROR_HANDLE_CONTROL_MESSAGE`).
- **Confirm the implementation exists before declaring it**: `lshal` registration, `ls /vendor/lib*/hw/`,
  and `strings` the daemon that should register it. On MT6768 `librilfusion.so` knows only
  `mtkradioex@2.0/2.1/2.2`; `@3.0` does not exist on this device.
- **The retry loop belongs to the client.** An APK probing a version that was never shipped (ImsService v13
  probing `mtkradioex@3.0` plus AIDL `IMtkRadioExIms`/`Voice`/`Modem`) retries once per second regardless
  of the manifest. Silence the client or ship the HAL; do not paper over it with declarations.
- Diff against the stock dump's `vendor/etc/vintf/manifest.xml` before adding or removing any instance.
- **Testing a manifest change**: stage it with `mount --bind`, then `adb reboot`. Since no live restart of
  the parser is safe, the reboot is the test — the bind mount is what makes it non-destructive.
  Host-side, run `assemble_vintf` + `checkvintf` first (`docs/validation.md`).
