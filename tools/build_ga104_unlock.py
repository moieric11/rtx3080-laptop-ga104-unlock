"""Cross-build the experimental EFI with LLVM in the existing WSL toolchain."""
import subprocess
import argparse
from pathlib import Path

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument("--ecc-readout",action="store_true")
p.add_argument("--cold-selector-variants",action="store_true")
p.add_argument("--cold-selector-mask",action="store_true")
args=p.parse_args()
if sum((args.ecc_readout,args.cold_selector_variants,args.cold_selector_mask))>1:p.error("Build variants separately")
out=Path("build/ga104-cold-selector-mask" if args.cold_selector_mask else "build/ga104-cold-selectors" if args.cold_selector_variants else "build/ga104-ecc-readout" if args.ecc_readout else "build/ga104-unlock")
(root/out).mkdir(parents=True,exist_ok=True)
sources=["ga104_unlock_app","discrete_handoff","probe_core","unlock/app_io",
         "unlock/format","unlock/runtime","unlock/sha256","unlock/checks","unlock/port",
         "unlock/compat","unlock/vendor/unlock_610","unlock/vendor/prep_610",
         "unlock/vendor/layout_610","unlock/vendor/vbios_610","unlock/vendor/gfw_eval"]
extra=["-DGU_ECC_READOUT_VARIANT"] if args.ecc_readout else []
if args.cold_selector_variants:extra.append("-DGU_COLD_SELECTOR_VARIANTS")
if args.cold_selector_mask:extra.append("-DGU_COLD_SELECTOR_MASK")
if args.ecc_readout:sources.append("ecc_readout")
objects=[]
for source in sources:
    obj=(out/(source.replace("/","_")+".obj")).as_posix()
    subprocess.run(["wsl","-e","clang","-target","x86_64-pc-windows-msvc","-std=c11",
                    "-ffreestanding","-fno-builtin","-fno-stack-protector","-mno-red-zone",
                    "-fshort-wchar","-Os","-Wall","-Wextra","-Werror","-Wno-unused-parameter",
                    "-D__KERNEL__","-DGU_UEFI_PORT","-DGU_DISCRETE_EXPERIMENTAL",
                    "-DGU_PROFILE_LENOVO_GA104","-DPROBE_LENOVO_GA104","-DGU_TARGET_PCI_ID=0x24dc10deU",
                    "-I","unlock/compat","-I","deps/edk2/MdePkg/Include","-I","deps/edk2/MdePkg/Include/X64",
                    "-c",source+".c","-o",obj]+extra,cwd=root,check=True)
    objects.append(obj)
subprocess.run(["wsl","-e","lld-link","/nologo","/nodefaultlib","/subsystem:efi_application",
                "/entry:efi_main","/machine:x64","/base:0","/dynamicbase","/nxcompat","/timestamp:0",
                "/opt:ref","/opt:icf","/map:"+(out/"unlock.map").as_posix(),"/out:"+(out/"GA104-UNLOCK.EFI").as_posix()]+objects,cwd=root,check=True)
subprocess.run(["python","unlock/verify_pe.py",str(out/"GA104-UNLOCK.EFI"),str(out/"unlock.map"),
                "--output",str(out/"verification.json")],cwd=root,check=True)
