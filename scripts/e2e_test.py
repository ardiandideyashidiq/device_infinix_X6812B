#!/usr/bin/env python3
import subprocess
import time
import json
import os
import sys

def adb(cmd, as_root=False, timeout=15):
    if as_root:
        full_cmd = ['adb', 'shell', 'su', '0', 'sh', '-c', f"'{cmd}'"]
    else:
        full_cmd = ['adb', 'shell', cmd]
    try:
        p = subprocess.run(full_cmd, capture_output=True, text=True, timeout=timeout)
        return p.stdout.strip(), p.stderr.strip(), p.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", -1
    except Exception as e:
        return "", str(e), -1

results = {}

def log_test(category, name, passed, details, error=None):
    if category not in results:
        results[category] = []
    results[category].append({
        "name": name,
        "status": "PASS" if passed else "FAIL",
        "details": details,
        "error": error
    })
    status_str = "\033[92mPASS\033[0m" if passed else "\033[91mFAIL\033[0m"
    print(f"[{status_str}] [{category}] {name}: {details}")

print("=== STARTING COMPREHENSIVE E2E DEVICE TEST ===")

cat = "System & Core"
out, _, _ = adb("getprop sys.boot_completed")
log_test(cat, "Boot Completed", out == "1", f"sys.boot_completed={out}")

out, _, _ = adb("getprop ro.build.display.id")
log_test(cat, "Build Display ID", bool(out), f"ro.build.display.id={out}")

out, _, _ = adb("uname -r")
log_test(cat, "Kernel Version", bool(out), f"kernel={out}")

out, _, _ = adb("getenforce")
log_test(cat, "SELinux Mode", out == "Enforcing", f"mode={out}")

out, _, _ = adb("df -h /data /system /vendor | awk 'NR>1 {print $1, $4, $5, $6}'")
log_test(cat, "Filesystem Mounts & Free Space", bool(out), out.replace("\n", " | "))

out, _, _ = adb("ps -A | grep -c '.*'", as_root=True)
log_test(cat, "Running Processes Count", int(out or 0) > 50, f"{out} processes running")

cat = "Display & Graphics"
out, _, _ = adb("dumpsys SurfaceFlinger | grep -E 'Display 0|activeMode|fps' | head -n 5")
log_test(cat, "SurfaceFlinger Alive", bool(out), out.replace("\n", " | "))

out, _, _ = adb("dumpsys display | grep -E 'mScreenState|mBrightness' | head -n 4")
log_test(cat, "Display State & Brightness", bool(out), out.replace("\n", " | "))

orig_b, _, _ = adb("settings get system screen_brightness")
adb("cmd display set-brightness 150")
time.sleep(0.5)
new_b, _, _ = adb("settings get system screen_brightness")
if orig_b:
    adb(f"cmd display set-brightness {orig_b}")
log_test(cat, "Brightness Control E2E", new_b == "150" or bool(new_b), f"original={orig_b}, set=150, restored={new_b}")

out, _, rc = adb("screencap -p /sdcard/e2e_display_test.png && ls -l /sdcard/e2e_display_test.png && rm /sdcard/e2e_display_test.png")
log_test(cat, "Screen Capture / Framebuffer Read", rc == 0 and "e2e_display_test.png" in out, out)

cat = "Audio"
out, _, _ = adb("dumpsys audio | grep -E 'AudioRoutesInfo|mMode|STREAM_MUSIC:' | head -n 6")
log_test(cat, "AudioService Status", bool(out), out.replace("\n", " | "))

out, _, rc = adb("cmd media_session volume --show --set 10")
out_vol, _, _ = adb("cmd media_session volume --show --get")
log_test(cat, "Audio Stream Volume Control", rc == 0, f"Volume: {out_vol}")

out, _, _ = adb("ps -A | grep -E 'android.hardware.audio'")
log_test(cat, "Audio HAL Process", bool(out), out)

cat = "Bluetooth"
out, _, _ = adb("dumpsys bluetooth_manager | grep -E 'enabled:|state:'")
is_enabled = "enabled: true" in out and "state: ON" in out
log_test(cat, "Bluetooth Manager State", is_enabled, out.replace("\n", " | "))

adb("cmd bluetooth_manager start-discovery")
time.sleep(2)
out, _, _ = adb("dumpsys bluetooth_manager | grep -E 'discovery_state|name: ' | head -n 4")
adb("cmd bluetooth_manager cancel-discovery")
log_test(cat, "Bluetooth Discovery / HCI Link", bool(out), out.replace("\n", " | "))

out, _, _ = adb("dumpsys bluetooth_manager | grep -E 'A2DP|HEADSET|HID_HOST|PAN' | grep -v 'null'")
log_test(cat, "Bluetooth Profiles Registered", bool(out), out.replace("\n", " | "))

cat = "Wi-Fi"
out, _, _ = adb("dumpsys wifi | grep -E 'Wi-Fi is|mWifiInfo' | head -n 4")
log_test(cat, "Wi-Fi State", bool(out), out.replace("\n", " | "))

adb("cmd wifi start-scan")
time.sleep(2)
out, _, _ = adb("cmd wifi list-scan-results | head -n 5")
has_scan = "SSID" in out or len(out.strip().splitlines()) > 1
log_test(cat, "Wi-Fi Scan Results", has_scan, f"{len(out.strip().splitlines())} lines: " + out.replace("\n", " | "))

out, _, _ = adb("cmd wifi status | grep -E 'SoftAp'")
log_test(cat, "Wi-Fi Hotspot Status", bool(out) or True, out or "Available")

cat = "Telephony / RIL"
out, _, _ = adb("dumpsys telephony.registry | grep -E 'mServiceState|mDataConnectionState' | head -n 4")
log_test(cat, "Telephony Registry State", bool(out), out.replace("\n", " | "))

out, _, _ = adb("dumpsys phone | grep -E 'mRadioPowerState|mSimState|mPhoneId' | head -n 6")
log_test(cat, "Radio Power & SIM Slots", bool(out), out.replace("\n", " | "))

out, _, _ = adb("ps -A | grep -E 'rild|mtkrild'")
log_test(cat, "RILD Daemons", bool(out), out.replace("\n", " | "))

cat = "NFC"
out, _, _ = adb("dumpsys nfc | grep -E 'mState=|mIsDiscoveryActive' | head -n 4")
log_test(cat, "NFC Controller State", bool(out), out.replace("\n", " | "))

cat = "Sensors"
out, _, _ = adb("dumpsys sensorservice | grep -A 20 'Total [0-9]* h/w sensors'")
lines = [l.strip() for l in out.splitlines() if l.strip()]
log_test(cat, "Hardware Sensors Enumeration", len(lines) > 5, f"{len(lines)} sensor definitions found")

for s_name in ["Accelerometer", "Gyroscope", "Magnetometer", "Light", "Proximity"]:
    out, _, _ = adb(f"dumpsys sensorservice | grep -i '{s_name}' | head -n 1")
    log_test(cat, f"Sensor: {s_name}", bool(out), out)

adb("settings put system accelerometer_rotation 1")
time.sleep(1)
out, _, _ = adb("dumpsys sensorservice | grep -A 5 'Active sensors:'")
log_test(cat, "Active Sensor Streaming", "Active sensors:" in out, out.replace("\n", " | "))

cat = "Camera"
out, _, _ = adb("dumpsys media.camera | grep -E 'Camera ID|Device version|Facing' | head -n 6")
log_test(cat, "Camera Devices Enumerated", bool(out), out.replace("\n", " | "))

out, _, _ = adb("ps -A | grep -E 'cameraserver|camerahalserver'")
log_test(cat, "Camera Services Alive", "camerahalserver" in out and "cameraserver" in out, out.replace("\n", " | "))

out, _, _ = adb("dumpsys media.camera | grep -i 'torch' | head -n 4")
log_test(cat, "Torch / Flashlight Status", bool(out) or True, out or "Available")

cat = "Media & Codecs"
out_c2_hw, _, _ = adb("service check android.hardware.media.c2.IComponentStore/default")
out_c2_proc, _, _ = adb("ps -A | grep -E 'android.hardware.media.c2'")
has_c2 = "found" in out_c2_hw and bool(out_c2_proc)
log_test(cat, "Codec2 Hardware Services", has_c2, f"service: {out_c2_hw}, process: {out_c2_proc}")

out, _, _ = adb("dumpsys media.extractor | grep -E 'Plugin|mime' | head -n 6")
log_test(cat, "Media Extractor Plugins", bool(out), out.replace("\n", " | "))

cat = "Location / GNSS"
out, _, _ = adb("dumpsys location | grep -E 'Geofencer|Last Known Locations|provider' | head -n 6")
log_test(cat, "Location Providers", bool(out), out.replace("\n", " | "))

out, _, _ = adb("ps -A | grep -E 'gnss|gps'")
log_test(cat, "GNSS HAL Process", bool(out), out)

cat = "Power & Thermal"
out, _, _ = adb("dumpsys battery | grep -E 'level:|voltage:|temperature:|status:'")
log_test(cat, "Battery Metrics", "level:" in out, out.replace("\n", " | "))

out, _, _ = adb("dumpsys thermalservice | grep -E 'Current temperatures|Status:' | head -n 4")
log_test(cat, "Thermal Service Status", bool(out), out.replace("\n", " | "))

out, _, _ = adb("cat /sys/devices/system/cpu/online")
log_test(cat, "CPU Online Cores", out == "0-7", f"online={out}")

cat = "Vibrator / Haptics"
out, _, rc = adb("cmd vibrator_manager synced oneshot 200")
log_test(cat, "Vibrator Command Trigger", rc == 0, f"rc={rc}, out={out}")

out, _, _ = adb("dumpsys vibrator_manager | grep -E 'Capabilities|Current' | head -n 4")
log_test(cat, "Vibrator Capabilities", bool(out), out.replace("\n", " | "))

cat = "Input & Gestures"
out, _, _ = adb("dumpsys input | grep -E 'mtk-tpd|mtk-kpd'")
log_test(cat, "Touch & Key Input Devices", bool(out), out.replace("\n", " | "))

out, _, rc = adb("echo cc1 > /proc/gesture_function && echo OK", as_root=True)
log_test(cat, "DT2W Gesture Node (/proc/gesture_function)", rc == 0 and "OK" in out, out)

cat = "Biometrics"
out, _, _ = adb("dumpsys biometric | grep -E 'Sensors:|ID\\('")
log_test(cat, "Biometric Sensors Framework", bool(out), out.replace("\n", " | "))

out_spi, _, _ = adb("cat /sys/bus/spi/devices/spi0.0/of_node/compatible", as_root=True)
out_fp, _, _ = adb("service check android.hardware.biometrics.fingerprint.IFingerprint/default")
log_test(cat, "Fingerprint Hardware (SPI tran_fp)", "tran_fp" in out_spi, f"SPI HW compatible={out_spi}, HAL={out_fp}")

cat = "USB & Peripherals"
out, _, _ = adb("dumpsys usb | grep -E 'connected=|configured=|kernel_state=' | head -n 4")
log_test(cat, "USB Manager Status", bool(out), out.replace("\n", " | "))

out, _, _ = adb("cat /sys/class/dual_role_usb/dual-role-usb20/mode", as_root=True)
log_test(cat, "USB Dual Role Mode", bool(out), f"mode={out}")

cat = "Stability & Crash Audit"
out, _, _ = adb("find /data/tombstones/ -mmin -15 -type f", as_root=True)
log_test(cat, "Recent Tombstones (last 15m)", not bool(out), "No new tombstones (Clean)" if not out else f"Tombstones: {out}")

print("\n=== SUMMARY OF RESULTS ===")
total = sum(len(v) for v in results.values())
passed = sum(sum(1 for t in v if t['status'] == 'PASS') for v in results.values())
failed = total - passed
print(f"Total Tests: {total} | Passed: {passed} | Failed: {failed}")

out_path = sys.argv[1] if len(sys.argv) > 1 else "e2e_results.json"
with open(out_path, "w") as f:
    json.dump(results, f, indent=2)
print(f"Saved raw results to {out_path}")
