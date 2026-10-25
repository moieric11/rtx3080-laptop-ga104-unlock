@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-handoff mkdir build\ga104-handoff
cl /nologo /std:c11 /W4 /WX /DPROBE_LENOVO_GA104 /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-handoff\ /Fe:build\ga104-handoff\test.exe test_discrete_handoff.c discrete_handoff.c probe_core.c
if errorlevel 1 exit /b 1
build\ga104-handoff\test.exe
exit /b %errorlevel%
