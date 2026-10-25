@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-probe mkdir build\ga104-probe
cl /nologo /c /std:c11 /O1 /GS- /Zl /Oi /DPROBE_LENOVO_GA104 /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-probe\ probe_core.c probe_app.c
if errorlevel 1 exit /b 1
cl /nologo /c /std:c11 /Od /GS- /Zl /Oi- /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-probe\ efi_runtime.c
if errorlevel 1 exit /b 1
rem MSVC LINK rejects DYNAMICBASE for EFI. Link a freestanding native image,
rem then change only its subsystem field; verify the final EFI afterwards.
link /nologo /nodefaultlib /subsystem:native /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /map:build\ga104-probe\probe.map /out:build\ga104-probe\GA104-PROBE.EFI build\ga104-probe\probe_core.obj build\ga104-probe\probe_app.obj build\ga104-probe\efi_runtime.obj
if errorlevel 1 exit /b 1
python tools\finalize_probe_efi.py build\ga104-probe\GA104-PROBE.EFI
if errorlevel 1 exit /b 1
python unlock\verify_pe.py build\ga104-probe\GA104-PROBE.EFI build\ga104-probe\probe.map --output build\ga104-probe\verification.json
exit /b %errorlevel%
