@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-handoff mkdir build\ga104-handoff
cl /nologo /c /std:c11 /O1 /GS- /Zl /Oi /W4 /DPROBE_LENOVO_GA104 /DPROBE_HANDOFF_TEST /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-handoff\ probe_core.c probe_app.c discrete_handoff.c
if errorlevel 1 exit /b 1
cl /nologo /c /std:c11 /Od /GS- /Zl /Oi- /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-handoff\ efi_runtime.c
if errorlevel 1 exit /b 1
link /nologo /nodefaultlib /subsystem:native /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /map:build\ga104-handoff\handoff.map /out:build\ga104-handoff\GA104-HANDOFF.EFI build\ga104-handoff\probe_core.obj build\ga104-handoff\probe_app.obj build\ga104-handoff\discrete_handoff.obj build\ga104-handoff\efi_runtime.obj
if errorlevel 1 exit /b 1
python tools\finalize_probe_efi.py build\ga104-handoff\GA104-HANDOFF.EFI
if errorlevel 1 exit /b 1
python unlock\verify_pe.py build\ga104-handoff\GA104-HANDOFF.EFI build\ga104-handoff\handoff.map --output build\ga104-handoff\verification.json
exit /b %errorlevel%
