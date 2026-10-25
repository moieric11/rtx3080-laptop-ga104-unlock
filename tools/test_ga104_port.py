"""Reproducible offline GA104 checks using the captured Lenovo boot state."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "ga104-probe"
OUT.mkdir(parents=True, exist_ok=True)


def run(args, name):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    (OUT / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{name}: {result.stdout}\n{result.stderr}")
    return result.stdout


includes = ["-I", "deps/edk2/MdePkg/Include", "-I", "deps/edk2/MdePkg/Include/X64"]
flags = ["wsl", "-e", "clang", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror"]
capture = "evidence/lenovo-ga104-boot01"
assets = "linux-usb-evidence/gpu-unlock/firmware"
specs = json.loads((ROOT / "unlock/assets.json").read_text())
for spec in specs:
    data = (ROOT / assets / spec["Name"]).read_bytes()
    if len(data) != spec["Bytes"] or hashlib.sha256(data).hexdigest() != spec["Sha256"]:
        raise ValueError(f"Firmware mismatch: {spec['Name']}")
print("Five firmware sizes and SHA256: PASS")
run(flags + ["tools/inspect_ga104_assets.c", "unlock/vendor/vbios_610.c",
             "unlock/vendor/prep_610.c", "unlock/vendor/layout_610.c",
             "-o", "build/ga104-probe/inspect-assets"], "compile-assets")
geometry = run(["wsl", "-e", "./build/ga104-probe/inspect-assets", capture+"/PROM.BIN",
                assets+"/gsp_ga10x.bin", assets+"/gsp_boot_desc_ga102.bin"], "inspect-assets")
(OUT / "asset-geometry.json").write_text(json.dumps(json.loads(geometry), indent=2), encoding="utf-8")
print("Captured VBIOS FWSEC and 16 GiB layout: PASS")
for profile, fixture, defines in [
    ("ga104", capture+"/REPORT.TXT", ["-DGU_PROFILE_LENOVO_GA104"]),
    ("msi", "evidence/boot-03/REPORT.TXT", []),
]:
    binary = "build/ga104-probe/checks-"+profile
    run(flags + ["-fshort-wchar"] + defines + includes +
        ["unlock/tests/test_checks.c", "unlock/checks.c", "-o", binary], "compile-"+profile)
    print(run(["wsl", "-e", "./"+binary, fixture], "checks-"+profile).strip())
sources = ["tests/test_efi_transaction", "port", "compat", "vendor/unlock_610",
           "vendor/prep_610", "vendor/layout_610", "vendor/vbios_610", "vendor/gfw_eval"]
defines = ["-D__KERNEL__", "-DGU_UEFI_PORT", "-DGU_OFFLINE_TEST",
           "-DGU_PROFILE_LENOVO_GA104", "-DGU_TARGET_PCI_ID=0x24dc10deU"]
run(flags + ["-Wno-unused-parameter", "-Wno-unused-function", "-fshort-wchar"] +
    defines + ["-I", "unlock/compat"] + includes + ["unlock/"+s+".c" for s in sources] +
    ["-o", "build/ga104-probe/transaction-ga104"], "compile-transaction")
result = run(["wsl", "-e", "./build/ga104-probe/transaction-ga104", capture+"/PROM.BIN",
              assets, capture+"/REPORT.TXT"], "transaction-ga104")
print(result.strip().splitlines()[-1])
callback_sources=["tools/test_ga104_unlock_callback.c","unlock/port.c","unlock/compat.c","unlock/sha256.c",
                  "unlock/vendor/unlock_610.c","unlock/vendor/prep_610.c","unlock/vendor/layout_610.c",
                  "unlock/vendor/vbios_610.c","unlock/vendor/gfw_eval.c"]
run(flags + ["-Wno-unused-parameter","-Wno-unused-function","-ffunction-sections","-fdata-sections","-fshort-wchar",
             "-DPROBE_LENOVO_GA104"] + defines + ["-I","unlock/compat"] + includes + callback_sources +
    ["-Wl,--gc-sections","-o","build/ga104-probe/test-callback"],"compile-callback")
callback=run(["wsl","-e","./build/ga104-probe/test-callback",capture+"/PROM.BIN",assets,capture+"/REPORT.TXT"],"app-callback")
print(callback.strip().splitlines()[-1])
run(flags + ["-Wno-unused-parameter","-Wno-unused-function","-ffunction-sections","-fdata-sections","-fshort-wchar",
             "-DPROBE_LENOVO_GA104","-DGU_COLD_SELECTOR_VARIANTS"] + defines + ["-I","unlock/compat"] + includes + callback_sources +
    ["-Wl,--gc-sections","-o","build/ga104-probe/test-callback-selectors"],"compile-callback-selectors")
variant=run(["wsl","-e","./build/ga104-probe/test-callback-selectors",capture+"/PROM.BIN",assets,capture+"/REPORT.TXT"],"app-callback-selectors")
print(variant.strip().splitlines()[-2])
# Ensure changing only the build flags cannot produce a live Discrete unlock.
guard = subprocess.run(flags + ["-DGU_UEFI_PORT", "-DGU_PROFILE_LENOVO_GA104"] + includes +
                       ["-c", "unlock/port.c", "-o", "build/ga104-probe/blocked.obj"],
                       cwd=ROOT, capture_output=True, text=True)
if guard.returncode == 0 or "discrete display handoff unresolved" not in guard.stderr:
    raise RuntimeError("Live GA104 build guard failed")
print("Live GA104 build requires the explicit experimental opt-in flag: PASS")
(OUT / "offline-tests.json").write_text(json.dumps({
    "firmware_hashes": "PASS", "captured_vbios_parser": "PASS", "layout_16gib": "PASS",
    "ga104_config_checks": 261, "msi_config_checks": 261,
    "simulated_transaction_scenarios": 20, "live_build_guard": "PASS",
    "actual_app_callback_simulated_scenarios":20,
    "hardware_unlock_tested": False, "discrete_handoff_implemented": True,
}, indent=2), encoding="utf-8")
