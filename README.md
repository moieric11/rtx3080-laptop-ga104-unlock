# RTX 3080 Laptop GA104 Compute Unlock

Experimental UEFI compute unlock for a **Lenovo RTX 3080 Laptop 16 GiB (GA104)**,
with Windows before/after benchmarks. Boot the USB, run one manually confirmed
operation, then continue to Windows in the same firmware session.

**[Download the complete USB package](https://github.com/moieric11/rtx3080-laptop-ga104-unlock/releases/tag/v0.4.1-experimental)**

**[Step-by-step installation tutorial](docs/INSTALL.md)**

The release ZIP includes `BOOTX64.EFI`, all five required firmware files,
a Windows USB copying assistant, English instructions, checksums, licenses and
corresponding source code. No compiler or Python installation is needed to use
the USB package. GitHub's automatic source-code download is not bootable.

## Tested hardware

| Component | Tested configuration |
| --- | --- |
| Laptop | Lenovo 82N6, AMD Ryzen 9 5900HX |
| GPU | RTX 3080 Laptop, GA104, 16 GiB |
| PCI / subsystem | `10de:24dc` / `17aa:3a58` |
| VBIOS | `94.04.3f.00.c5` |
| Graphics mode | Discrete |
| Windows benchmark environment | NVIDIA 617.14, CUDA 13.4 |

Other laptops, boards and firmware versions have not been validated. The
application checks the exact board, firmware and cold GPU state. Do not bypass
those checks to force another GPU through.

This is experimental firmware-level software. Display loss and a forced
power-off are possible. It does not flash the VBIOS or install a Windows driver.
Persistence across restart, sleep/resume and long-term stability are not
established. It does not enable ECC.

## Quick start

1. Read the [installation tutorial](docs/INSTALL.md) and check the hardware.
2. Download and extract the complete release ZIP, then verify its SHA256.
3. Copy its contents to the root of a FAT32 UEFI USB, or run
   `Install-USB.ps1 -DriveLetter F` from the extracted package with your USB letter.
4. Fully shut down, then boot the USB in Discrete mode with Secure Boot disabled.
5. After the checks pass, press **U once**. Keep the USB connected.
6. After success and display recovery, select **Windows Boot Manager without
   rebooting**. Measure your workload under matching before/after conditions.

Esc or a 30-second timeout cancels. If the screen stays black for more than
three minutes, or a `STOP` message appears, power off with the physical button.
Reports are written to `RTX3080-UNLOCK/RUNxx/REPORT.TXT` on the USB.

## Measured Windows results

Matched October 23 runs used the same executable and CUDA/cuBLAS versions, with
three processes per condition. These are dense GEMM measurements with FP16
inputs and FP32 accumulation.

| Matrix size | Stock TFLOPS | Unlocked TFLOPS | Change |
| --- | ---: | ---: | ---: |
| 2048 | 38.50 | 58.20 | +51.2% |
| 4096 | 40.40 | 71.80 | +77.7% |
| 8192 | 42.26 | 73.04 | +72.8% |

FP16 accumulation control measurements stayed close to their stock values.
These results do not establish gaming gains or the same improvement in every
AI application. See the [stock runs](evidence/windows-baseline-2026-10-23),
[unlocked runs](evidence/windows-after-2026-10-23),
[UEFI report](evidence/lenovo-ga104-unlock01/REPORT.TXT) and
[precision comparison](GA104-PRECISION-SUITE.md).

## Package contents and firmware

`BOOTX64.EFI` is the UEFI application. It also requires the five exact files
listed in [unlock/assets.json](unlock/assets.json), provided under
`RTX3080-UNLOCK/ASSETS` in the release. They supply the GPU-side Booter, signature,
GSP image, bootstrap and descriptor used during the transaction. Their names
include `ga102` as expected by the existing GA104 path; do not rename them.

The published v0.4.1 release uses the `GU_COLD_SELECTOR_VARIANTS` build, accepting only three
observed cold selector states on this Lenovo while retaining the other checks.
Its EFI SHA256 is
`4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000`.
The same build bytes were reproduced for this release. The newly assembled
archive and copying assistant have not themselves undergone a fresh hardware
boot test. See [release notes](docs/RELEASE-NOTES.md).

## Build from source

For the UEFI build, install Python 3.9+ on Windows and clang/lld in WSL. Run from
the repository root on a Windows drive accessible to WSL:

```powershell
python .\tools\build_ga104_unlock.py --cold-selector-mask
```

Output: `build/ga104-cold-selector-mask/GA104-UNLOCK.EFI`, with PE verification JSON.
This development variant fixes the RUN14-16 preflight refusals: SS0 must be
nonzero with all eight override-enable bits clear, rather than equal one of
three captures. SS1 must still equal 6 and all other checks remain mandatory.
Offline tests do not establish hardware validation of newly accepted states.
See [the SS0 correction](GA104-COLD-SELECTORS.md).
Use `--cold-selector-variants` to reproduce the published v0.4.1 binary;
its existing release packager remains pinned to that binary.
The builder invokes `wsl -e clang` and `wsl -e lld-link` and verifies an AMD64 EFI
application without imports. The reproduced release used LLVM 21.1.8.

To assemble a complete release using separately supplied, matching assets:

```powershell
python .\tools\package_ga104_release.py --assets C:\path\to\firmware --output .\dist\v0.4.1
```

The packager requires a clean, committed source checkout, the reviewed EFI hash
and all five exact firmware hashes. It embeds `git archive HEAD` as `SOURCE.zip`
and rejects an existing output directory. It never writes to a USB.

`python tools/test_ga104_port.py` runs offline checks with local firmware and
VBIOS fixtures; private VBIOS captures are not included in this public source.
The Windows benchmark scripts additionally require MSVC and CUDA, and some
retain development-machine paths. [check-unlock.cmd](CHECK-UNLOCK.md) is an
optional performance heuristic requiring the exact reference executable.

The inherited MSI desktop target is separate from this laptop port.
[Its original USB instructions](docs/USB-BOOT.md) do not describe the GA104
release procedure.

## Attribution and license

Based on [rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public)
and [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock).
The port and reusable unlock core are **GPL-2.0-only**. EDK2 and other third-party
components retain their own notices; binary firmware is not relicensed as GPL.
See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[LICENSES](LICENSES). Corresponding source is included with the release.

`PUBLIC-SOURCE-MANIFEST.json` records hashes from the latest reviewed source
export. For the published release, use the package's `SHA256-MANIFEST.json`
and `BUILD.json`.

For bug reports, include PCI/subsystem IDs, VBIOS and driver versions, build
hashes, reproduction steps and a reviewed UEFI report. Remove personal paths,
serial numbers, GPU UUIDs and credentials from attachments.
