@echo off
REM ============================================================================
REM build.bat - Build the 64-bit Windows nd500x.exe using w64devkit.
REM
REM Usage:
REM   build.bat              - release build (default)
REM   build.bat debug        - debug build
REM   build.bat clean        - delete build_release\ and build\
REM
REM Requirements:
REM   - w64devkit (64-bit) extracted somewhere on disk.
REM     Default path: C:\Utils\w64devkit
REM     Override by setting the W64DEVKIT env var before running this script:
REM       set W64DEVKIT=D:\tools\w64devkit
REM       build.bat
REM
REM   - No other dependency. cJSON is downloaded automatically by CMake when it
REM     is not already installed, and readline is optional (its absence only
REM     costs tab completion in the debugger REPL).
REM
REM Notes on what the Windows build does NOT include:
REM   - DAP (the VS Code debug adapter) is off: libdap is POSIX-only. The
REM     built-in --debug REPL works normally.
REM
REM This script does NOT permanently modify your PATH. It only prepends
REM w64devkit\bin for the duration of this build.
REM ============================================================================

setlocal enableextensions enabledelayedexpansion

REM --- Locate w64devkit ------------------------------------------------------
if "%W64DEVKIT%"=="" set "W64DEVKIT=C:\Utils\w64devkit"

if not exist "%W64DEVKIT%\bin\gcc.exe" (
    echo ERROR: w64devkit not found at "%W64DEVKIT%".
    echo.
    echo Either install w64devkit there, or point this script at your install:
    echo     set W64DEVKIT=D:\path\to\w64devkit
    echo     build.bat
    exit /b 1
)

REM Prepend w64devkit\bin so gcc, make, cmake, ninja and sh resolve to
REM w64devkit first (ahead of any other MinGW / MSYS2 / Cygwin installs already
REM on PATH, which would otherwise mix toolchains).
set "PATH=%W64DEVKIT%\bin;%PATH%"

REM --- Sanity-check the toolchain --------------------------------------------
where gcc   >nul 2>&1 || ( echo ERROR: gcc not found on PATH after adding w64devkit.   & exit /b 1 )
where cmake >nul 2>&1 || ( echo ERROR: cmake not found on PATH after adding w64devkit. & exit /b 1 )
where ninja >nul 2>&1 || ( echo ERROR: ninja not found on PATH after adding w64devkit. & exit /b 1 )

echo.
echo Using w64devkit at: %W64DEVKIT%
for /f "tokens=*" %%v in ('gcc --version ^| findstr /r "^gcc"') do echo   %%v
echo.

REM --- Pick the target -------------------------------------------------------
set "TARGET=release"
if /i "%~1"=="debug"   set "TARGET=debug"
if /i "%~1"=="release" set "TARGET=release"
if /i "%~1"=="clean"   set "TARGET=clean"

if "%TARGET%"=="clean" (
    echo Cleaning build directories...
    if exist build_release rmdir /s /q build_release
    if exist build         rmdir /s /q build
    echo Done.
    exit /b 0
)

if "%TARGET%"=="release" (
    set "OUTDIR=build_release"
    set "BUILDTYPE=Release"
) else (
    set "OUTDIR=build"
    set "BUILDTYPE=Debug"
)

REM --- Configure -------------------------------------------------------------
REM -G Ninja is required, not a preference: left to itself CMake picks the
REM Visual Studio generator on Windows, which needs MSVC and cannot drive a
REM pure MinGW toolchain.
echo Configuring %BUILDTYPE% build in %OUTDIR%\ ...
cmake -S . -B %OUTDIR% -G Ninja -DCMAKE_BUILD_TYPE=%BUILDTYPE%
if errorlevel 1 (
    echo.
    echo CONFIGURE FAILED.
    exit /b 1
)

REM --- Build -----------------------------------------------------------------
echo Building %TARGET%...
cmake --build %OUTDIR%
if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)

echo.
echo ============================================================
echo Build complete.
echo   Binary: %OUTDIR%\bin\nd500x.exe
echo.
echo Run it:
echo   Boot NDIX from a disk image:
echo     %OUTDIR%\bin\nd500x.exe --ndix rootfs_full.img
echo   SINTRAN shell:
echo     %OUTDIR%\bin\nd500x.exe --monitor
echo   Debugger REPL:
echo     %OUTDIR%\bin\nd500x.exe --debug
echo ============================================================

endlocal
