# AGENTS.md

Guidance for coding agents working on the Infinix X6812B (MT6768) device tree.

## Project overview

- Target: AxionOS 2.9 (`NOESIS`) on LineageOS 23.2 (Android 16 QPR2, release `bp4a`), userdebug.
- Product: `lineage_X6812B` (Axion keeps the lineage_ product prefix); lunch as
  `axion X6812B userdebug [va|pico|core|gms]`.
- SoC: MediaTek MT6768 (Helio G85 class), kernel 4.14.186, aarch64 + 32-bit.
- `$ANDROID_ROOT` is `../../..` from this tree (`/home/rd/axion`).
- Stock dump: `/home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30`
- Tree history: forked from `realme_RMX2020`, sepolicy replaced from mt6781 X670 (`9529c41`).

Key paths:
- `device/infinix/X6812B` (this tree) | `device/infinix/X6812B-kernel` (prebuilt Image.gz, DTB, headers)
- `hardware/mediatek` (shared MTK HALs) | `device/mediatek/sepolicy_vndr` (shared MTK sepolicy)
- `device/axion/common` (Axion product config; `config/version.mk` holds the 2.9 version props)
- `vendor/lineage` (Axion fork of the Lineage vendor tree — product inheritance lives here)
- `vendor/infinix/X6812B` (generated — never edit)

## Hard rules (Non-negotiable)

1. **Never run a build.** Ask user to build and report `out/error.log`.
2. **Never propose "rebuild the kernel" as a fix.** Kernel source is unavailable; prebuilt only.
3. **Never run `grep`/`rg` at `$ANDROID_ROOT` or on `out/`.** Scope to specific subdirectories.
4. **Never edit AOSP/Lineage/Axion source or `vendor/infinix/X6812B`.** Fixes belong in this tree.
   One exception, and only when the user asks for it: the `fuck-bpf` series, applied with
   `/home/rd/axion/fuck-bpf/apply.py --mb` from the source root (BPF reverts needed to boot Android 16
   QPR2 on this 4.14 kernel past `bpfloader-failed`). Never edit those repos by hand.
5. **Never `git reset --hard`.** Use `git checkout -- <path>` only on touched files.
6. **No explanatory comments in code/config files.** Put reasoning in commit messages.
7. **Check AOSP, MediaTek and Axion upstream before adding anything.** Avoid duplicate
   configs/props/services.
8. **Subsystem work requires reading playbooks.** Before editing audio, wifi/tethering, usb, telephony,
   battery, display, touch, sensors, or camera/media, agent MUST read `docs/playbooks/subsystems.md`.
9. **Validate host-side before the device sees it.** Manifest, property, `file_contexts` and `.rc` edits
   get their validator first — `docs/validation.md`. `xmllint --noout` proves well-formed XML, not a
   bootable manifest.
10. **Never overwrite a working system file wholesale, and never restart the framework to "test" something.**
    Stage with `mount --bind`, reboot to apply. `ctl.restart hwservicemanager` / `zygote` / `stop; start`
    take down the display and framework with no live recovery. Read `docs/live-testing.md` before any
    device write.

## Debugging workflow (Order of attack)

1. **GitHub search first**: Search commit history & repos for the error (`search_commits` / `gh search commits '<error>'`). Most MTK/Lineage problems are already solved.
2. **Read AOSP / MediaTek / Axion source**: Trace the exact error message, struct, or service tag.
3. **Compare sibling trees**: Inspect `mt6768-dev/android_device_xiaomi_mt6768-common` and the Axion
   `device/axion/common` product config.
4. **Reverse-engineer stock**: Check stock dump props, `.rc`, XMLs, and ELF blobs.
5. **Test live on device**: see `docs/live-testing.md` for the rules and revert paths.

Android 16 specifics for this device: the prebuilt 4.14 kernel has no BPF support usable by the QPR2
platform, so `bpfloader` pinning fails and the boot loops; the `fuck-bpf` reverts are load-bearing, not
optional. Android 16 also reparented swaths of the tree (sepolicy vndr, shared MTK HALs, AIDL migration
of HALs that were HIDL on 22.2). Treat every 22.2-era `PRODUCT_PACKAGES` entry and `.rc` service as
unverified until it resolves in the new source.

## Essential commands

```bash
# Regenerate vendor tree after ANY proprietary-files.txt change (must finish before building)
./extract-files.py /home/rd/Projects/android/custom-rom/x6812b/Infinix-Hot-11S-NFC-X6812B-30

# Refresh pinned-blob sha1s (must leave `git diff` clean on second run)
python3 update-sha1sums.py
```

## Documentation index

- `docs/validation.md`: Host validation recipes — sepolicy neverallows, VINTF `assemble_vintf`/`checkvintf`,
  `checkfc`, `host_init_verifier`, `xmllint`, `check_elf_file`, SONAME audit, single-binary ninja compile.
- `docs/live-testing.md`: Live device rules — parse-at-boot table, bind-mount staging, which services are
  safe to restart, recovery paths, VINTF phantom-declaration behaviour.
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
