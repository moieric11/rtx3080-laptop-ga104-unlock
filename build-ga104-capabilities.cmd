@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-research mkdir build\ga104-research
set "CUDA_DIR=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4"
cl /nologo /EHsc /std:c++17 /O2 /I "%CUDA_DIR%\include" /Fo:build\ga104-research\ /Fe:build\ga104-research\capabilities.exe tools\ga104_capabilities.cpp /link /LIBPATH:"%CUDA_DIR%\lib\x64" cudart.lib
exit /b %errorlevel%
