"""Prepare a verified ECC observation overlay for the existing known USB."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
build=root/"build/ga104-ecc-readout"
package=build/"usb-overlay"
usb=Path("F:/")
expected="5e5b99107f6ddf38fdda50a7995b60cb3c335742cfa6f7d8df4f519fd3c353fa"
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst)
    if sha(src)!=sha(dst):raise ValueError("Copy mismatch: "+str(dst))
old=root/"build/ga104-unlock/GA104-UNLOCK.EFI"
if sha(old)!=expected or sha(usb/"EFI/BOOT/BOOTX64.EFI")!=expected:raise ValueError("Unexpected base image")
verification=json.loads((build/"verification.json").read_text())
if sha(build/"GA104-UNLOCK.EFI")!=verification["sha256"]:raise ValueError("Unverified ECC image")
for spec in json.loads((root/"unlock/assets.json").read_text()):
    f=usb/"RTX3080-UNLOCK/ASSETS"/spec["Name"]
    if f.stat().st_size!=spec["Bytes"] or sha(f)!=spec["Sha256"]:raise ValueError("USB asset mismatch")
copy(build/"GA104-UNLOCK.EFI",package/"EFI/BOOT/BOOTX64.EFI")
copy(old,package/"EFI/TOOLS/GA104-UNLOCK-VALIDE.EFI")
copy(root/"GA104-ECC-READOUT.md",package/"ECC-LIRE-MOI.md")
(package/"RESTAURER-UNLOCK-VALIDE.cmd").write_text('@echo off\ncopy /y "%~dp0EFI\\TOOLS\\GA104-UNLOCK-VALIDE.EFI" "%~dp0EFI\\BOOT\\BOOTX64.EFI"\nif errorlevel 1 exit /b 1\necho Unlock de calcul valide restaure.\n',encoding="ascii")
sources=[]
for name in ("ga104_unlock_app.c","ecc_readout.c","ecc_readout.h","test_ecc_readout.c","test-ecc-readout.cmd",
             "GA104-ECC-READOUT.md","tools/build_ga104_unlock.py","tools/package_ecc_readout.py"):
    sources.append(root/name)
basezip=root/"ga104-unlock-package/SOURCE.zip"
replaced={p.relative_to(root).as_posix() for p in sources}
with zipfile.ZipFile(basezip) as base,zipfile.ZipFile(package/"SOURCE-ECC.zip","w",zipfile.ZIP_DEFLATED) as out:
    for name in base.namelist():
        if name not in replaced:out.writestr(name,base.read(name))
    for path in sources:out.write(path,path.relative_to(root).as_posix())
manifest={"variant":"ecc-readout-v1","verification":verification,"base_validated_efi_sha256":expected,
    "ecc_register_writes":0,"ecc_activation_attempted":False,"compute_unlock_on_U":True,
    "new_read_offset":"0x00823814","readout_bit":16,"hardware_readout_tested":False,"collector_simulated_cases":16,
    "nvidia_source_commit":"ca2d03e6fafd4d4b56b383497d931c25b283bfcf",
    "sources_sha256":{p.relative_to(root).as_posix():sha(p) for p in sources},
    "overlay_requires_existing_validated_usb_assets":True}
(package/"ECC-BUILD.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
with zipfile.ZipFile(build/"GA104-ECC-READOUT-OVERLAY.zip","w",zipfile.ZIP_DEFLATED) as out:
    for path in package.rglob("*"):
        if path.is_file():out.write(path,path.relative_to(package).as_posix())
with zipfile.ZipFile(build/"GA104-ECC-READOUT-OVERLAY.zip") as archive:
    for path in package.rglob("*"):
        if path.is_file() and archive.read(path.relative_to(package).as_posix())!=path.read_bytes():raise ValueError("ZIP mismatch")
# Preserve reports and deploy the boot image last.
files=[p for p in package.rglob("*") if p.is_file()]
boot=package/"EFI/BOOT/BOOTX64.EFI"
for path in files:
    if path!=boot:copy(path,usb/path.relative_to(package))
copy(boot,usb/"EFI/BOOT/BOOTX64.EFI")
if sha(usb/"EFI/TOOLS/GA104-UNLOCK-VALIDE.EFI")!=expected:raise ValueError("Missing validated fallback")
print("ECC observation overlay verified and installed on F:. Validated fallback and old reports preserved.")
