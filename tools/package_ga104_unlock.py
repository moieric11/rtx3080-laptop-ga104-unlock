"""Prepare and verify the local experimental USB package; optional known USB update."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

p=argparse.ArgumentParser()
p.add_argument("--usb",type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
package=root/"ga104-unlock-package"
build=root/"build/ga104-unlock"
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(src,dst)
    if digest(src)!=digest(dst):raise ValueError("Copy hash mismatch: "+str(dst))
report=root/"evidence/lenovo-ga104-handoff01/HANDOFF.TXT"
lines=report.read_text().splitlines()
events={lines[i]:(int(lines[i+1].split("=",1)[1],16),int(lines[i+2].split("=",1)[1],16))
        for i in range(len(lines)-2) if lines[i+1].startswith("status=") and lines[i+2].startswith("value=")}
for name in ("disconnect-result","exclusive-roundtrip","reconnect-result","display-restored","handoff-final"):
    if events[name][0] or (name in ("exclusive-roundtrip","display-restored","handoff-final") and events[name][1]!=1):
        raise ValueError("Hardware handoff prerequisite missing")
verification=json.loads((build/"verification.json").read_text())
if digest(build/"GA104-UNLOCK.EFI")!=verification["sha256"]:raise ValueError("EFI verification mismatch")
copy(build/"GA104-UNLOCK.EFI",package/"EFI/BOOT/BOOTX64.EFI")
copy(build/"GA104-UNLOCK.EFI",package/"EFI/TOOLS/GA104-UNLOCK.EFI")
copy(root/"evidence/lenovo-ga104-handoff01/TEST-T.EFI",package/"EFI/TOOLS/GA104-HANDOFF.EFI")
for spec in json.loads((root/"unlock/assets.json").read_text()):
    source=root/"linux-usb-evidence/gpu-unlock/firmware"/spec["Name"]
    if source.stat().st_size!=spec["Bytes"] or digest(source)!=spec["Sha256"]:raise ValueError("Wrong asset")
    copy(source,package/"RTX3080-UNLOCK/ASSETS"/spec["Name"])
for name in ("GA104-PORTAGE.md","LICENSE","THIRD_PARTY_NOTICES.md"):
    copy(root/name,package/("LIRE-MOI.md" if name=="GA104-PORTAGE.md" else name))
(package/"RESTAURER-TEST-T.cmd").write_text('@echo off\ncopy /y "%~dp0EFI\\TOOLS\\GA104-HANDOFF.EFI" "%~dp0EFI\\BOOT\\BOOTX64.EFI"\nif errorlevel 1 exit /b 1\necho Test T restaure.\n',encoding="ascii")
source_files=[]
allowed={".c",".h",".cpp",".py",".cmd",".ps1",".md",".json",".txt"}
for path in root.iterdir():
    if path.is_file() and (path.suffix in allowed or path.name in ("LICENSE",".gitignore")):source_files.append(path)
for name in ("unlock","tools","deps","docs","LICENSES"):
    for path in root.joinpath(name).rglob("*"):
        if path.is_file() and not {"build","releases","__pycache__"}.intersection(path.relative_to(root/name).parts) and path.suffix in allowed:
            source_files.append(path)
with zipfile.ZipFile(package/"SOURCE.zip","w",zipfile.ZIP_DEFLATED,compresslevel=1) as z:
    for path in sorted(source_files):z.write(path,path.relative_to(root).as_posix())
manifest={"version":"ga104-discrete-v0.4-experimental","hardware_unlock_tested":False,
          "hardware_handoff_tested":True,"handoff_report_sha256":digest(report),
          "verification":verification,"offline_tests":json.loads((root/"build/ga104-probe/offline-tests.json").read_text()),
          "source_sha256":{p.relative_to(root).as_posix():digest(p) for p in sorted(source_files)}}
(package/"BUILD.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
with zipfile.ZipFile(build/"GA104-UNLOCK-USB.zip","w",zipfile.ZIP_DEFLATED,compresslevel=1) as z:
    for path in sorted(package.rglob("*")):
        if path.is_file():z.write(path,path.relative_to(package).as_posix())
with zipfile.ZipFile(build/"GA104-UNLOCK-USB.zip") as z:
    for path in package.rglob("*"):
        if path.is_file() and z.read(path.relative_to(package).as_posix())!=path.read_bytes():raise ValueError("ZIP mismatch")
if a.usb:
    usb=a.usb.resolve()
    if str(usb).lower()!="f:\\" or not usb.joinpath("RTX3080-PROBE/RUN02/HANDOFF.TXT").is_file():raise ValueError("Unexpected USB root")
    if digest(usb/"EFI/BOOT/BOOTX64.EFI")!=digest(root/"evidence/lenovo-ga104-handoff01/TEST-T.EFI"):raise ValueError("USB boot image changed")
    if usb.joinpath("RTX3080-UNLOCK").exists():raise ValueError("Unlock output already exists; inspect before updating")
    for path in package.rglob("*"):
        if path.is_file():copy(path,usb/path.relative_to(package))
    print("USB F: updated and every copied file SHA256 verified")
print("Experimental package ZIP verified:",build/"GA104-UNLOCK-USB.zip")
