REM ./build.bat Debug x64
REM ./build.bat for all platforms

@echo off
setlocal enabledelayedexpansion

echo Building NppVim plugin...

REM Check if MSBuild is available
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MSBuild not found in PATH
    pause
    exit /b 1
)

REM Set default configuration and platform if not provided
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set PLATFORM=%2
if "%PLATFORM%"=="" set PLATFORM=All

set PROJECT_FILE=NppVim.vcxproj
if not exist "%PROJECT_FILE%" (
    echo ERROR: Project file %PROJECT_FILE% not found
    pause
    exit /b 1
)

echo Using project file: %PROJECT_FILE%
echo Configuration: %CONFIG%
echo Platform: %PLATFORM%

REM Build based on platform selection
if /i "%PLATFORM%"=="All" (
    echo.
    echo Building for Win32...
    msbuild "%PROJECT_FILE%" /p:Configuration=%CONFIG% /p:Platform=Win32 /m
    if !errorlevel! neq 0 (
        echo ERROR: Build failed for Win32
        pause
        exit /b !errorlevel!
    )

    echo.
    echo Building for x64...
    msbuild "%PROJECT_FILE%" /p:Configuration=%CONFIG% /p:Platform=x64 /m
    if !errorlevel! neq 0 (
        echo ERROR: Build failed for x64
        pause
        exit /b !errorlevel!
    )

    echo.
    echo Building for ARM64...
    msbuild "%PROJECT_FILE%" /p:Configuration=%CONFIG% /p:Platform=ARM64 /m
    if !errorlevel! neq 0 (
        echo ERROR: Build failed for ARM64
        pause
        exit /b !errorlevel!
    )

    echo.
    echo All platforms built successfully!
) else (
    echo.
    echo Building for %PLATFORM%...
    msbuild "%PROJECT_FILE%" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m
    if !errorlevel! neq 0 (
        echo ERROR: Build failed for %PLATFORM%
        pause
        exit /b !errorlevel!
    )
    echo %PLATFORM% built successfully!
)

echo.
echo Build completed!
pause
