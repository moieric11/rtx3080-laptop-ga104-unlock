@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-repo-benchmark mkdir build\ga104-repo-benchmark
set "CUDA_DIR=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4"
cl /nologo /EHsc /std:c++17 /O2 /I "%CUDA_DIR%\include" /Fo:build\ga104-repo-benchmark\ /Fe:build\ga104-repo-benchmark\ga104-repo-gemm.exe tools\ga104_repo_gemm.cpp /link /LIBPATH:"%CUDA_DIR%\lib\x64" cublas.lib cudart.lib
exit /b %errorlevel%
