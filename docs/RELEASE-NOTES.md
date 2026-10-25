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
