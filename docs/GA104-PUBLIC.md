# RTX 3080 Laptop GA104 compute unlock — experimental Windows port

This repository contains a hardware-specific UEFI port and Windows compute
measurements for a Lenovo RTX 3080 Laptop. It builds on
[rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public)
and [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock).
The original authors retain their copyright and attribution.

## Tested hardware

Lenovo 82N6, Ryzen 9 5900HX, Discrete graphics mode, RTX 3080 Laptop 16 GiB
(GA104), PCI `10de:24dc`, subsystem `17aa:3a58`, VBIOS `94.04.3f.00.c5`.
Windows measurements used NVIDIA driver 617.14 and CUDA 13.4.
Other laptops and GPUs have not been validated by this port.

This is experimental firmware-level work. Display loss and a forced power-off
are possible. The code checks the board, firmware, registers and cleanup state;
do not remove those checks to force a different GPU through. The unlock path
does not flash the VBIOS. Persistence across restart, sleep and resume and
long-term stability are not established.

## Measured results

The matched October 23 Windows runs used the same executable and CUDA/cuBLAS
versions, with three processes per condition. These are dense GEMM results,
FP16 inputs with FP32 accumulation, not game or general application speedups.

| Matrix | Before, TFLOPS | After, TFLOPS | Change |
| --- | ---: | ---: | ---: |
| 2048 | 38.50 | 58.20 | +51.2% |
| 4096 | 40.40 | 71.80 | +77.7% |
| 8192 | 42.26 | 73.04 | +72.8% |

FP16 accumulation control measurements stayed close to their previous values.
The raw runs and comparison are in
[`evidence/windows-baseline-2026-10-23`](../evidence/windows-baseline-2026-10-23)
and [`evidence/windows-after-2026-10-23`](../evidence/windows-after-2026-10-23).
The UEFI report is
[`evidence/lenovo-ga104-unlock01/REPORT.TXT`](../evidence/lenovo-ga104-unlock01/REPORT.TXT).
The larger precision comparison is documented in
[`GA104-PRECISION-SUITE.md`](../GA104-PRECISION-SUITE.md).

## Source layout and build

- `ga104_unlock_app.c`: GA104 UEFI application and preconditions.
- `discrete_handoff.c`: detach and reconnect the UEFI display driver.
- `unlock/board_profile.h`: explicit board profiles.
- `tools/build_ga104_unlock.py`: experimental GA104 cross-build using clang and
  lld-link in WSL. Run from Windows with `python tools/build_ga104_unlock.py`.
- `tools/test_ga104_port.py`: offline checks using local firmware and VBIOS
  fixtures. These private binary fixtures are not included in the public export.
- `build-ga104-benchmark.cmd`: Windows GEMM build using local MSVC and CUDA.

Build scripts currently contain paths for the development machine. Adjust the
Visual Studio and CUDA locations for your installation. The source export does
not contain firmware blobs, VBIOS dumps, EFI executables or ready-to-boot USB
images. `unlock/assets.json` records the expected firmware filenames, sizes
and hashes. They must be supplied separately; a source checkout is not a
bootable package. The local packaging scripts depend on development fixtures
and are not a portable public installer.

The original MSI desktop build remains a separate target. Its instructions
and hardware assumptions are in the original README below the GA104 notice;
do not apply those desktop instructions as a validated laptop procedure.

## Check the current Windows performance profile

[`check-unlock.cmd`](../check-unlock.cmd) compares three runs of the original
GEMM executable against the matched stock and unlocked references.
It reports probably active, stock profile or indeterminate.
This is a performance heuristic, not a live read of the hardware switch.
See [`CHECK-UNLOCK.md`](../CHECK-UNLOCK.md).

The exact reference executable is not included in this source export. A rebuilt
executable is deliberately rejected if its SHA256 differs. Collect a new matched
stock/unlocked pair and review the checker references before using a new binary;
do not bypass the hash check and assume the old measurements remain comparable.

## Contributing

For a bug report, include GPU PCI/subsystem IDs, VBIOS and driver versions,
the commit/build variant, reproduction steps and a reviewed UEFI report.
Remove serial numbers, GPU UUIDs, personal paths and credentials from attachments.
Distinguish an operator-supplied state label from register observations and
measured performance. For a speedup claim, provide matching before/after runs
with executable hashes, library versions and power/temperature conditions.

Changes supporting another board should preserve the existing checks and add
an explicit profile with offline coverage. A successful mock test does not
establish hardware compatibility.

## License and attribution

The port and reusable unlock core are **GPL-2.0-only**. See
[`LICENSE`](../LICENSE), [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)
and [`LICENSES`](../LICENSES) for upstream and EDK2 notices.
Binary asset redistribution is outside this source export.

`PUBLIC-SOURCE-MANIFEST.json` describes this exported snapshot. The inherited
`SOURCE-MANIFEST.json` describes the older upstream snapshot. Published reports
may have user home paths and GPU UUIDs redacted; use the public manifest to
check the exported file bytes.
