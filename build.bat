@echo off
set cfg=%1
if "%cfg%"=="" goto debug
if /I "%cfg%"=="release" goto release
echo Invalid argument: %cfg%
exit /b 1

:debug
call "C:\dev\msvc\VC\Auxiliary\Build\vcvars64.bat"
if not exist debug mkdir debug
cd debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
cd ..
exit /b

:release
REM ---- Win32 ----
call "C:\dev\msvc\VC\Auxiliary\Build\vcvars32.bat"
if not exist build-win32 mkdir build-win32
cd build-win32
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd ..

REM ---- x64 ----
call "C:\dev\msvc\VC\Auxiliary\Build\vcvars64.bat"
if not exist build-x64 mkdir build-x64
cd build-x64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd ..

REM ---- ARM64 ----
call "C:\dev\msvc\VC\Auxiliary\Build\vcvarsarm64.bat"
if not exist build-arm64 mkdir build-arm64
cd build-arm64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd ..
exit /b
