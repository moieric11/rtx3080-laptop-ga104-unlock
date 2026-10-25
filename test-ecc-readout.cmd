@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-ecc-readout mkdir build\ga104-ecc-readout
cl /nologo /std:c11 /W4 /WX /I deps\edk2\MdePkg\Include /I deps\edk2\MdePkg\Include\X64 /Fo:build\ga104-ecc-readout\ /Fe:build\ga104-ecc-readout\test.exe test_ecc_readout.c ecc_readout.c
if errorlevel 1 exit /b 1
build\ga104-ecc-readout\test.exe
exit /b %errorlevel%
