# USB installation and boot tutorial

This guide is for the experimental **Lenovo 82N6 / RTX 3080 Laptop GA104**
profile. It is not a universal RTX 3080 installer. No Windows service, driver
patch or VBIOS flash is installed.

## 1. Check the hardware and prepare

The tested system uses a Ryzen 9 5900HX, RTX 3080 Laptop 16 GiB, PCI
`10de:24dc`, subsystem `17aa:3a58`, VBIOS `94.04.3f.00.c5`, and **Discrete**
graphics mode. Windows measurements used NVIDIA 617.14 and CUDA 13.4.
Check your IDs in Device Manager > Display adapters > Properties > Details >
Hardware Ids. Matching the product name alone does not establish compatibility.

Keep the laptop on AC power. Back up the USB contents before preparing it.
Use an x64 UEFI-bootable FAT32 USB with at least 128 MiB free. Disable Secure
Boot for this unsigned EFI application and use Discrete graphics mode.
Before changing firmware settings, have your BitLocker/device-encryption recovery
key available. The package does not change these settings for you.

## 2. Download and verify

Download `GA104-UNLOCK-USB-v0.4.1-experimental.zip` and `SHA256SUMS.txt` from
the [v0.4.1 experimental release](https://github.com/moieric11/rtx3080-laptop-ga104-unlock/releases/tag/v0.4.1-experimental).
Use the release asset, rather than GitHub's automatic source-code ZIP.

```powershell
Get-FileHash .\GA104-UNLOCK-USB-v0.4.1-experimental.zip -Algorithm SHA256
```

Compare the hash with `SHA256SUMS.txt`, then extract the archive. In the extracted
folder, optionally verify every packaged file before copying:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-USB.ps1 -VerifyOnly
```

The package contains the compiled EFI, all five required firmware assets,
English instructions, license notices, a SHA256 manifest, and corresponding
source code in `SOURCE.zip`. Python, WSL and a compiler are not required to use
the package.

## 3. Copy to the USB

Copy **the contents of the extracted folder** to the USB root. For example, if
the USB is `F:`, these files must exist:

```text
F:\EFI\BOOT\BOOTX64.EFI
F:\RTX3080-UNLOCK\ASSETS\booter_load_ga102.bin
F:\RTX3080-UNLOCK\ASSETS\v67-ga102-610.43.03.bin
F:\RTX3080-UNLOCK\ASSETS\gsp_ga10x.bin
F:\RTX3080-UNLOCK\ASSETS\gsp_boot_ga102.bin
F:\RTX3080-UNLOCK\ASSETS\gsp_boot_desc_ga102.bin
```

Alternatively, use the copying assistant from the extracted folder, replacing
`F` with the drive letter you checked in Explorer:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-USB.ps1 -DriveLetter F
```

The assistant verifies package hashes, requires a FAT32 USB volume, refuses an
existing `EFI\BOOT\BOOTX64.EFI` or conflicting package files, copies the files,
and verifies the destination. It does not format disks. Keep existing boot media
separate; use another USB if it refuses a conflict.

## 4. Boot and run once

1. Fully shut down the laptop, then power on into its one-time UEFI boot menu.
2. Select the USB UEFI entry while using Discrete graphics mode.
3. The application checks the board, PCI configuration, VBIOS/PROM, cold GPU
   state and the five firmware files. If it rejects a check, stop and inspect
   its report. Do not bypass checks or substitute arbitrary firmware versions.
4. At the prompt, press **U once**. Esc or a 30-second timeout cancels.
5. Keep the USB connected. The display may go black while the UEFI display
   driver is detached. If it remains black for more than three minutes, power
   off with the physical button. If a `STOP` message appears, power off and do
   not continue to Windows.
6. After success and display recovery, select **Windows Boot Manager without
   rebooting** in the same firmware session.

Success should end with selectors `SS0=0x88888888`, `SS1=0x8`, and a report line
containing `FINAL GA104 status=0`, `display_restored=1`, `retained=0`.
Reports are saved as `RTX3080-UNLOCK\RUNxx\REPORT.TXT` on the USB. Check the newest
run. Successful selectors alone do not prove a Windows performance gain.

## 5. Measure and return to normal boot

Measure the same CUDA workload before and after, with the same executable,
libraries, power mode and comparable temperature. The published improvements
are dense GEMM measurements, not a promise of gaming or general AI speedups.
The optional source-tree `check-unlock.cmd` requires Python, the exact reference
benchmark executable and a compatible CUDA environment; it is not required to
boot this package. See [CHECK-UNLOCK.md](https://github.com/moieric11/rtx3080-laptop-ga104-unlock/blob/main/CHECK-UNLOCK.md).

To stop using the tool, remove the USB and boot Windows normally. No installed
Windows component needs uninstalling. Restart, power-off, sleep and resume can
invalidate the unlock; persistence and long-term stability are not established.
Use a full shutdown for a fresh attempt. Do not assume a driver reload restores
the original state.

## Troubleshooting

| Symptom | Action |
| --- | --- |
| USB not listed | Check x64 UEFI boot support, FAT32, and the root `EFI\BOOT\BOOTX64.EFI` path. |
| Security violation | Check that Secure Boot is disabled for this unsigned application. |
| Missing assets or hash failure | Extract the complete release ZIP again; preserve filenames and directory layout. |
| Rejected before U | Inspect the newest report; hardware or cold-state checks did not pass. |
| Screen remains black / STOP | Power off as described above; preserve the USB report for investigation. |
| No Windows speedup | Verify matching benchmarks and conditions; the tested microbenchmark gain may not apply to your workload. |

## Why the five firmware files are included

`BOOTX64.EFI` orchestrates the operation. The GPU-side sequence also needs:

| File | Purpose |
| --- | --- |
| `booter_load_ga102.bin` | Booter program executed by a GPU microcontroller. |
| `v67-ga102-610.43.03.bin` | Signature data used by the unlock sequence. |
| `gsp_ga10x.bin` | GSP firmware image and original signature data. |
| `gsp_boot_ga102.bin` | GSP bootstrap program. |
| `gsp_boot_desc_ga102.bin` | Bootstrap image layout descriptor. |

The `ga102` filenames are expected by this GA104 port. The files are loaded
into memory for the procedure; they are not flashed into the VBIOS. Exact sizes
and hashes are recorded in `unlock/assets.json` inside `SOURCE.zip`.
