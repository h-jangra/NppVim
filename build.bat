@echo off
setlocal enabledelayedexpansion

if "%1"=="" (
    echo Building Debug...
    call "C:\dev\msvc\VC\Auxiliary\Build\vcvarsall.bat" x64

    if not exist build\debug mkdir build\debug
    pushd build\debug

    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..\..
    cmake --build .

    popd
    echo Debug build completed in build/debug/
    goto :eof
)


if "%1"=="release" (
    goto :BUILD_RELEASE
)

echo Usage: build.bat [release]
goto :eof


:BUILD_RELEASE
echo Cleaning distribution folders...
for %%A in (NppVim NppVim.x64 NppVim.arm64) do (
    if exist "%%A" rmdir /s /q "%%A"
)

echo Building x64 Release...
call "C:\dev\msvc\VC\Auxiliary\Build\vcvarsall.bat" x64
if not exist build\x64 mkdir build\x64
pushd build\x64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..\..
cmake --build .
popd

echo Building win32 Release...
call "C:\dev\msvc\VC\Auxiliary\Build\vcvarsall.bat" x86
if not exist build\win32 mkdir build\win32
pushd build\win32
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..\..
cmake --build .
popd

echo Building arm64 Release...
call "C:\dev\msvc\VC\Auxiliary\Build\vcvarsall.bat" x64_arm64
if not exist build\arm64 mkdir build\arm64
pushd build\arm64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..\..
cmake --build .
popd

echo Release build complete.
goto :eof
