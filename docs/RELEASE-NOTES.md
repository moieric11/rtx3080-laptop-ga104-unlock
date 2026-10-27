# Development update: SS0 override mask (October 27, 2026)

The optional `--cold-selector-mask` build replaces the three-value SS0 list
with a nonzero value check and an eight-bit override-enable mask. This addresses
the preflight refusals captured in RUN14 (`0x61014101`) and RUN15–16
(`0x60004100`). SS1 must still equal 6; all other checks and manual confirmation
remain mandatory. The transaction and display recovery code are unchanged.

Offline callback tests cover the five observed SS0 values, rejection of each
override-enable bit, invalid reads and the remaining cold-state checks.
Hardware execution is simulated; the v2 variant still requires a hardware boot.
See [selector details](../GA104-COLD-SELECTORS.md) and the
[captured reports](../evidence/usb-boot-2026-10-27).

`tools/update_ga104_selector_usb.ps1` migrates a known v1 FAT32 USB after
validating its image and firmware hashes, and saves a restorable v1 backup.

The published v0.4.1 package and its pinned EFI hash remain the v1 variant.
Use `--cold-selector-variants` to reproduce that release.

# v0.4.1-experimental

First complete English-documented GA104 USB release for the existing Lenovo
82N6 profile. Includes the five hash-checked firmware assets, compiled EFI,
FAT32 USB copying assistant, English tutorial, licenses and corresponding source.

The application is the existing cold-selector variant, not a new unlock
algorithm. Rebuilt with LLVM 21.1.8, it reproduces SHA256
`4b58554e18b0a2e987040cf3c006aa122bfd84ccd0f2dbee11cb225da7340000`.
It accepts only the observed SS0 values `0x61014100`, `0x60014100`,
`0x61004100`, each with SS1 `6`; the other board and cold-state checks remain.

The captured report in
[RUN05](../evidence/lenovo-ga104-cold-selectors-success01/RUN05/REPORT.TXT)
records a successful transaction, display recovery and final SS0 `0x88888888`
/ SS1 `8`. Historical application banners and research notes predate that
report and retain their original wording about unverified recovery. This does
not establish persistence or long-term stability.

Release validation covers a reproducible EFI build, PE structure, exact firmware
sizes and SHA256, source archive inclusion, ZIP integrity and packaged-file
hashes. Offline hardware/firmware tests are simulations. The newly assembled
release archive and copying assistant have not undergone a fresh hardware boot.

No VBIOS flash, ECC activation, Windows driver installation or support for
additional boards is introduced.
