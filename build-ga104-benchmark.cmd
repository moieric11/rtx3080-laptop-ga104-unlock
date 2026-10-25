@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-unlock mkdir build\ga104-unlock
set "CUDA_DIR=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4"
cl /nologo /EHsc /std:c++17 /O2 /I "%CUDA_DIR%\include" /Fo:build\ga104-unlock\ /Fe:build\ga104-unlock\ga104-gemm.exe tools\ga104_gemm_bench.cpp /link /LIBPATH:"%CUDA_DIR%\lib\x64" cublas.lib cudart.lib
exit /b %errorlevel%
