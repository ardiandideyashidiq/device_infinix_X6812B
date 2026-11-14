#!/usr/bin/env python3
import argparse
import re
import subprocess
import sys
import time

APPS = [
    ("Settings", "com.android.settings", "com.android.settings/.Settings"),
    ("Dialer", "com.android.dialer", "com.android.dialer/.main.impl.MainActivity"),
    ("Jelly", "org.lineageos.jelly", "org.lineageos.jelly/.MainActivity"),
    ("Messaging", "com.android.messaging", "com.android.messaging/.ui.conversationlist.ConversationListActivity"),
    ("Calculator", "com.android.calculator2", "com.android.calculator2/.Calculator"),
]
LAUNCHER_PKG = "com.android.launcher3"

def run_adb(cmd):
    full_cmd = ["adb", "shell"] + cmd
    res = subprocess.run(full_cmd, capture_output=True, text=True)
    return res.stdout.strip()

def reset_gfxinfo(pkgs):
    for pkg in pkgs:
        run_adb(["dumpsys", "gfxinfo", pkg, "reset"])

def get_gfx_stats(pkg):
    out = run_adb(["dumpsys", "gfxinfo", pkg])
    total_m = re.search(r"Total frames rendered:\s+(\d+)", out)
    jank_m = re.search(r"Janky frames:\s+(\d+)\s+\(([\d\.]+)%\)", out)
    p50_m = re.search(r"50th percentile:\s+(\d+)ms", out)
    p90_m = re.search(r"90th percentile:\s+(\d+)ms", out)
    p95_m = re.search(r"95th percentile:\s+(\d+)ms", out)
    p99_m = re.search(r"99th percentile:\s+(\d+)ms", out)

    total = int(total_m.group(1)) if total_m else 0
    janky = int(jank_m.group(1)) if jank_m else 0
    jank_pct = float(jank_m.group(2)) if jank_m else 0.0
    p50 = int(p50_m.group(1)) if p50_m else 0
    p90 = int(p90_m.group(1)) if p90_m else 0
    p95 = int(p95_m.group(1)) if p95_m else 0
    p99 = int(p99_m.group(1)) if p99_m else 0

    return {
        "pkg": pkg,
        "total": total,
        "janky": janky,
        "jank_pct": jank_pct,
        "p50": p50,
        "p90": p90,
        "p95": p95,
        "p99": p99,
    }

def main():
    parser = argparse.ArgumentParser(description="Stress test app open/close cycles and capture jank stats.")
    parser.add_argument("--cycles", type=int, default=5, help="Number of open/close cycles per app (default: 5)")
    parser.add_argument("--delay", type=float, default=1.0, help="Delay in seconds between actions (default: 1.0)")
    args = parser.parse_args()

    # Wake and unlock device
    run_adb(["input", "keyevent", "KEYCODE_WAKEUP"])
    run_adb(["wm", "dismiss-keyguard"])
    run_adb(["input", "keyevent", "KEYCODE_HOME"])
    time.sleep(0.5)

    all_pkgs = [pkg for _, pkg, _ in APPS] + [LAUNCHER_PKG]
    reset_gfxinfo(all_pkgs)

    print(f"Running {args.cycles} open/close cycles for {len(APPS)} apps + Launcher3 tracking...")

    for cycle in range(1, args.cycles + 1):
        for name, pkg, component in APPS:
            run_adb(["am", "start", "-n", component])
            time.sleep(args.delay)
            run_adb(["input", "keyevent", "KEYCODE_HOME"])
            time.sleep(args.delay)

    # Collect stats
    results = []
    for name, pkg, _ in APPS:
        stats = get_gfx_stats(pkg)
        stats["name"] = name
        results.append(stats)

    launcher_stats = get_gfx_stats(LAUNCHER_PKG)
    launcher_stats["name"] = "Launcher3"
    results.append(launcher_stats)

    # Print summary table
    print("\n" + "=" * 78)
    print(f"{'Target':<14} | {'Total':<7} | {'Janky':<7} | {'Jank %':<8} | {'p50':<5} | {'p90':<5} | {'p95':<5} | {'p99':<5}")
    print("-" * 78)
    for r in results:
        print(f"{r['name']:<14} | {r['total']:<7} | {r['janky']:<7} | {r['jank_pct']:<7.2f}% | {r['p50']:<3}ms | {r['p90']:<3}ms | {r['p95']:<3}ms | {r['p99']:<3}ms")
    print("=" * 78)

if __name__ == "__main__":
    main()
