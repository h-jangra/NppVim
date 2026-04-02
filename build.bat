@echo off
set NPP="C:\Program Files\Notepad++\notepad++.exe"
set DIR="C:\Program Files\Notepad++\plugins\NppVim"

call "C:\dev\msvc\setup_x64.bat"
cmake -S . -B build\x64 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\x64

taskkill /IM notepad++.exe /F >nul 2>&1
if not exist %DIR% mkdir %DIR%
copy /Y build\x64\NppVim.dll %DIR%

start "" %NPP%