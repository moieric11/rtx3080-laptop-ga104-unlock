# Third-party notices

## GPU unlock core and UEFI port - GPL-2.0-only

- Upstream: [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956)
- Pinned commit: `0ebdb1c6308b6a2072be5bf04513472942541956`.
- Files: `unlock/vendor/` implementations of `unlock_610`, `prep_610`,
  `layout_610`, `vbios_610`, `gfw_eval`, and `diagnostic.h`.
- License: [LICENSE](LICENSE).

The original Windows UEFI port comes from
[rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public).
Original authors retain their copyright and attribution.

October 18, 2026 changes selected the MSI RTX 3080 target, connected checked
UEFI I/O and execution/cleanup state, and added FWSEC diagnostics.
`tests/reference/unlock_610_v06.c` is the local v0.6 snapshot used for comparisons.
The UEFI port and host/mock tests are also GPL-2.0-only. Version 0.10 manages
Bus Master through PCI I/O Attributes and checks protocol state against the
actual PCI Command register.

October 19, 2026 changes organized public tools and documentation: local USB
packaging, public test runner, and relative-path build metadata, preserving the
GPU execution conditions and transaction sequence.

The Lenovo GA104 laptop port adds explicit board checks, Discrete display
handoff/reconnection, and corresponding offline tests. This release adds
English documentation and complete USB packaging without changing the existing
cold-selector unlock algorithm.

## EDK2 headers - BSD-2-Clause-Patent

- Upstream: [TianoCore EDK2](https://github.com/tianocore/edk2/tree/c1415771ac592ad8509351c46cd3cda4f38d7d52)
- Pinned commit: `c1415771ac592ad8509351c46cd3cda4f38d7d52`.
- Files: `deps/edk2/MdePkg/Include/`.
- License and notices: [EDK2 license](LICENSES/EDK2-BSD-2-Clause-Patent.txt)
  and each file's header.

The application uses header declarations and does not link EDK2 library binaries.

## NVIDIA firmware and inherited notices

The complete USB release contains the five binary assets identified by exact
filenames, sizes and SHA256 values in `unlock/assets.json` in the source tree.
These were supplied from the local `gpu-unlock/firmware` asset set and are
preserved byte-for-byte. Their presence in the package does not relicense them
under GPL. Preserve the included NVIDIA license and notices with the binaries.

The upstream source and EFI reference notices are retained:

- [NVIDIA MIT notice](LICENSES/NVIDIA-MIT.txt)
- [CMP40HX MIT notice](LICENSES/CMP40HX-MIT.txt)
- [NVIDIA Driver License Agreement](LICENSES/NVIDIA-Driver.txt)

Keep applicable licenses and copyright notices when copying the source or USB
package. Corresponding GPL source is provided as `SOURCE.zip` in the release.
