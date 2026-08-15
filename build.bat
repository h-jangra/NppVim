@echo off
setlocal

set NPP="C:\Program Files\Notepad++\notepad++.exe"
set DIR="C:\Program Files\Notepad++\plugins\NppVim"

if "%1"=="tag" goto tag

call "C:\dev\msvc\setup_x64.bat"

cmake -S . -B build\x64 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\x64 || exit /b 1

taskkill /IM notepad++.exe /F >nul 2>&1

if not exist %DIR% mkdir %DIR%
if not exist %DIR%\docs mkdir %DIR%\docs

copy /Y build\x64\NppVim.dll %DIR%
copy /Y docs\* %DIR%\docs\

start "" %NPP%
exit /b 0

:tag
set VERSION=%2

if "%VERSION%"=="" exit /b 1

set RC_VERSION=

for /f "tokens=2" %%A in ('findstr "FILEVERSION" plugin\NppVim.rc') do set RC_VERSION=%%A

set RC_VERSION=%RC_VERSION:,=.%

if not "%VERSION%"=="%RC_VERSION%" (
    echo Version mismatch: %VERSION% != %RC_VERSION%
    exit /b 1
)

git tag %VERSION%
git push origin %VERSION%
