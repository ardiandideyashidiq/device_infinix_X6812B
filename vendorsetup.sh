#!/bin/bash

# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0

root="${ANDROID_BUILD_TOP:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
d="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# patch series: <patch file>:<target repo relative to root>
series=(
    "0001-NetBpfLoad-do-not-abort-on-absent-bpf-jit-sysctls.patch:packages/modules/Connectivity"
    "0002-NetBpfLoad-treat-load-failures-as-non-fatal.patch:packages/modules/Connectivity"
    "0003-bpfloader-do-not-exit-when-elf-load-fails.patch:system/bpf"
    "0010-system-bpf-Prepare-for-the-future-making-bpfloader.rs-fails-all.patch:system/bpf"
    "0011-system-core-libprocessgroup-tolerate-ENOENT-on-cgroup.procs-write.patch:system/core"
    "0012-system-netd-Support-no-bpf-usecase.patch:system/netd"
    "0013-system-netd-Don-t-abort-in-case-of-cgroup-bpf-setup-fail-since-s.patch:system/netd"
    "0014-system-netd-fix-iptables-COMMIT-PING-parse-error-in-tethering-CT.patch:system/netd"
    "0015-system-netd-make-dnsmasq-communication-failures-non-fatal-for-tethering.patch:system/netd"
    "0016-system-vold-vold-use-sdcardfs-as-fallback-when-FUSE-BPF-is-unavailable.patch:system/vold"
    "0017-system-sepolicy-sepolicy-ship-plat_sepolicy_genfs_202404.cil-for-legacy-vendor-partitions.patch:system/sepolicy"
    "0019-system-apex-Disable-O_Direct.patch:system/apex"
    "0020-packages-modules-Connectivity-Allow-failing-to-load-bpf-programs-for-BPF-less-devi.patch:packages/modules/Connectivity"
    "0021-packages-modules-Connectivity-Revert-drop-support-for-V-gsi-on-pixel-5-R-base-kern.patch:packages/modules/Connectivity"
    "0022-packages-modules-Connectivity-Revert-netdupdatable-add-back-abort-on-init-fail.patch:packages/modules/Connectivity"
    "0023-packages-modules-Connectivity-Additional-bpf-prevent-crash.patch:packages/modules/Connectivity"
    "0024-packages-modules-Connectivity-Tethering-Fix-allow-clients-to-use-VPN-toggle-post-Q.patch:packages/modules/Connectivity"
    "0025-packages-modules-Connectivity-Tethering-fall-back-to-modern-DNS-resolver-when-dnsmasq-is-unavailable.patch:packages/modules/Connectivity"
    "0026-kernel-configs-kernel_configs-Relax-old-kernel-reqs.patch:kernel/configs"
    "0027-kernel-configs-kernel_configs-Remove-BPF-requirements-from-restored-q-android-4.9.patch:kernel/configs"
    "0028-bionic-epoll_pwait2-fall-back-to-epoll_pwait-on-kernels-5.11.patch:bionic"
    "0029-bionic-make-property-area-init-failures-non-fatal-for-old-vendors.patch:bionic"
)

mode=""
case "${1:-}" in
    --patch) mode="apply" ;;
    --revert) mode="revert" ;;
    "") exit 0 ;;
    *) echo "usage: $(basename "$0") [--patch|--revert]" >&2; exit 2 ;;
esac

failed=0
applied=0
skipped=0
reverted=0

apply_patch() {
    local patch="$1" repo="$2" name
    name="$(basename "$patch" .patch)"

    if [ ! -f "$patch" ]; then
        echo "[patch] $name... FAILED (file not found)"
        failed=$((failed + 1))
        return
    fi

    if [ ! -d "$repo/.git" ]; then
        echo "[patch] $name... FAILED (target repo not found: $repo)"
        failed=$((failed + 1))
        return
    fi

    if git -C "$repo" apply --reverse --check --ignore-whitespace "$patch" 2>/dev/null; then
        echo "[patch] $name... already applied"
        skipped=$((skipped + 1))
        return
    fi

    if git -C "$repo" apply --check --ignore-whitespace "$patch" 2>/dev/null; then
        if git -C "$repo" apply --ignore-whitespace "$patch"; then
            echo "[patch] $name... applied"
            applied=$((applied + 1))
        else
            echo "[patch] $name... FAILED (apply error)"
            failed=$((failed + 1))
        fi
        return
    fi

    echo "[patch] $name... FAILED (context mismatch, patch needs rebasing)"
    failed=$((failed + 1))
}

revert_patch() {
    local patch="$1" repo="$2" name
    name="$(basename "$patch" .patch)"

    if [ ! -f "$patch" ] || [ ! -d "$repo/.git" ]; then
        echo "[patch] $name... SKIPPED (missing)"
        return
    fi

    if git -C "$repo" apply --reverse --check --ignore-whitespace "$patch" 2>/dev/null; then
        if git -C "$repo" apply --reverse --ignore-whitespace "$patch"; then
            echo "[patch] $name... reverted"
            reverted=$((reverted + 1))
            return
        fi
    fi
    echo "[patch] $name... not applied"
    skipped=$((skipped + 1))
}

for entry in "${series[@]}"; do
    name="${entry%%:*}"
    repo="${entry#*:}"
    if [ "$mode" = "revert" ]; then
        revert_patch "$d/patches/$name" "$root/$repo"
    else
        apply_patch "$d/patches/$name" "$root/$repo"
    fi
done

if [ "$mode" = "revert" ]; then
    echo "==> $reverted reverted, $skipped not applied"
else
    echo "==> $applied applied, $skipped already applied, $failed failed"
fi

if [ "$failed" -ne 0 ]; then
    echo "error: $failed patch(es) failed to apply; refusing to continue" >&2
    echo "       rebase them onto $root before building" >&2
    exit 1
fi
