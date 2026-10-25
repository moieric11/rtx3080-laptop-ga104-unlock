@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ga104-precision-suite mkdir build\ga104-precision-suite
set "CUDA_DIR=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4"
"%CUDA_DIR%\bin\nvcc.exe" -std=c++17 -O3 -arch=sm_86 -allow-unsupported-compiler -Xcompiler /EHsc,/Zc:preprocessor,/Zc:__cplusplus -I analysis\research\cutlass\include tools\ga104_precision_suite.cu -o build\ga104-precision-suite\ga104-precision-suite.exe -lcublas
exit /b %errorlevel%
