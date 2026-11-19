@echo off
chcp 65001 >nul 2>&1
setlocal EnableDelayedExpansion
title xrVR installer

set "ROOT=%~dp0"
cd /d "%ROOT%"

echo.
echo ============================================================
echo   xrVR - VR mod for S.T.A.L.K.E.R. Shadow of Chernobyl
echo ============================================================
echo.

if "%~1"=="" (
    set "ENGINE=%ROOT%OGSR-Engine"
) else (
    set "ENGINE=%~1"
)
echo Engine target: !ENGINE!
echo.

REM ---------------------------------------------------------------- VS
echo [1/6] Searching Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD="

if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
        set "MSBUILD=%%i"
    )
)

if not defined MSBUILD (
    echo     vswhere failed, checking default paths...
    for %%E in (2022 2019 2026) do (
        for %%T in (Community Professional Enterprise BuildTools) do (
            if not defined MSBUILD (
                if exist "%ProgramFiles%\Microsoft Visual Studio\%%E\%%T\MSBuild\Current\Bin\MSBuild.exe" (
                    set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\%%E\%%T\MSBuild\Current\Bin\MSBuild.exe"
                )
            )
        )
    )
)

if not defined MSBUILD (
    echo.
    echo     ERROR: Visual Studio not found.
    echo     Install Visual Studio 2022 Community with
    echo     "Desktop development with C++" and run again.
    echo.
    pause
    exit /b 1
)
echo     OK: !MSBUILD!
echo.

REM ---------------------------------------------------------------- PYTHON
echo [2/6] Checking Python...

set "PY="
where python >nul 2>&1 && set "PY=python"
if not defined PY ( where py >nul 2>&1 && set "PY=py" )

if not defined PY (
    echo.
    echo     ERROR: Python not found.
    echo     Install Python 3.8+ from python.org and tick
    echo     "Add python.exe to PATH". Then run again.
    echo.
    pause
    exit /b 1
)
for /f "tokens=*" %%v in ('%PY% --version 2^>^&1') do echo     OK: %%v
echo.

REM ---------------------------------------------------------------- ENGINE
echo [3/6] OGSR Engine...

if exist "!ENGINE!\Engine.sln" (
    echo     Already present: !ENGINE!
) else (
    echo     Cloning OGSR-Engine, about 139 MB, please wait...
    git clone --depth 1 --filter=blob:none https://github.com/OGSR/OGSR-Engine.git "!ENGINE!"
    if errorlevel 1 (
        echo.
        echo     ERROR: clone failed. Check git and internet.
        echo     Or clone manually and pass the path:
        echo         install.bat C:\path\to\OGSR-Engine
        echo.
        pause
        exit /b 1
    )
    pushd "!ENGINE!"
    git sparse-checkout init --cone
    git sparse-checkout set ogsr_engine Game 3rd_party
    popd
)
echo.

REM ---------------------------------------------------------------- PATCHES
echo [4/6] Applying VR patches...

%PY% tools\apply_patches.py "!ENGINE!"
if errorlevel 1 (
    echo.
    echo     ERROR: patching failed, see text above.
    echo.
    pause
    exit /b 1
)
echo.

REM ---------------------------------------------------------------- BUILD
echo [5/6] Building Release x64...
echo     First build takes 15-40 minutes. Do not close this window.
echo.

"!MSBUILD!" "!ENGINE!\Engine.sln" /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 (
    echo.
    echo     ERROR: build failed, see errors above.
    echo     Install Windows SDK via Visual Studio Installer if missing.
    echo.
    pause
    exit /b 1
)
echo.

REM ---------------------------------------------------------------- RESULT
echo [6/6] Done.
echo.

set "BINDIR=!ENGINE!\bin"
if not exist "!BINDIR!\xrEngine.exe" (
    for /d %%d in ("!ENGINE!\*") do (
        if exist "%%d\xrEngine.exe" set "BINDIR=%%d"
    )
)

if exist "!BINDIR!\xrEngine.exe" (
    echo     Built: !BINDIR!\xrEngine.exe
) else (
    echo     xrEngine.exe not found, look for it in !ENGINE!
)

echo.
echo ============================================================
echo   MANUAL STEPS LEFT (3 minutes)
echo ============================================================
echo.
echo   1. Copy openvr_api.dll from SteamVR:
echo         Steam\steamapps\common\SteamVR\bin\win64\openvr_api.dll
echo      into:
echo         !BINDIR!
echo.
echo   2. Copy gamedata\ from licensed SoC 1.0006 into the same folder
echo      (configs, scripts, meshes, textures, sounds, levels).
echo.
echo   3. Copy vr_config.ltx into:
echo         !BINDIR!\gamedata\configs\vr\
echo.
echo   4. Start SteamVR, connect Pico, run xrEngine.exe.
echo.
echo   If VR does not start, type vr_status in the game console.
echo.
echo ============================================================
echo.

pause
