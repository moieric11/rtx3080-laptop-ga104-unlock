@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-probe mkdir build\ga104-probe
cl /nologo /std:c11 /W3 /DPROBE_LENOVO_GA104 /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-probe\ /Fe:build\ga104-probe\test.exe test_core.c probe_core.c probe_app.c
if errorlevel 1 exit /b 1
build\ga104-probe\test.exe
exit /b %errorlevel%
