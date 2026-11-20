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
echo [3/7] OGSR Engine...

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
echo [4/7] Updating third-party components...
echo     OGSR needs 12 extra git repos. This takes a few minutes.
pushd "!ENGINE!"

if exist "!ENGINE!\Update_Components.cmd" (
    echo     Running Update_Components.cmd from the engine repo...
    call "!ENGINE!\Update_Components.cmd" < nul
) else (
    echo     Update_Components.cmd not found, cloning manually...
    call :clone microsoft/DirectXTex.git mar2025 3rd_party\Src\DirectXTex\DirectXTex
    call :clone OGSR/DirectXMesh.git main 3rd_party\Src\DirectXMesh\DirectXMesh
    call :clone microsoft/DirectXMath.git main 3rd_party\Src\DirectXMath\DirectXMath
    call :clone cameron314/concurrentqueue.git master 3rd_party\Src\concurrentqueue\concurrentqueue
    call :clone AgentD/squashfs-tools-ng.git master 3rd_party\Src\libsquashfs\squashfs-tools-ng
    call :clone lz4/lz4.git dev 3rd_party\Src\lz4\lz4
    call :clone facebook/zstd.git dev 3rd_party\Src\zstd\zstd
    call :clone microsoft/mimalloc.git v3.5.1 3rd_party\Src\mimalloc\mimalloc
    call :clone NVIDIA/DLSS.git v310.7.0 3rd_party\Src\NVIDIA_DLSS\DLSS
    call :clone tzcnt/cpputils.git main 3rd_party\Src\cpputils\cpputils
    call :clone OGSR/discord-rpc.git master 3rd_party\Src\DiscordRPC\DiscordRPC
    call :clone Tencent/rapidjson.git v1.1.0 3rd_party\Src\DiscordRPC\DiscordRPC\thirdparty\rapidjson-1.1.0
    call :clone OGSR/FidelityFX-SDK.git release-FSR3-3.1.2-DX11-Native-API 3rd_party\Src\FidelityFX-SDK\FidelityFX-SDK
)
popd
echo     Components done.

echo [5/7] Applying VR patches...

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
echo [6/7] Building Release x64...
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
echo [7/7] Copying openvr_api.dll next to the engine...
set "OVRDLL=!ENGINE!\ogsr_engine\xrVR\openvr\lib\openvr_api.dll"
if exist "!OVRDLL!" (
    if exist "!ENGINE!\bin_x64" copy /y "!OVRDLL!" "!ENGINE!\bin_x64\openvr_api.dll" >nul 2>&1
    if exist "!ENGINE!\bin" copy /y "!OVRDLL!" "!ENGINE!\bin\openvr_api.dll" >nul 2>&1
    echo     copied openvr_api.dll
) else (
    echo     WARN: openvr_api.dll not found, copy it near xrEngine.exe manually
)

echo [7/7] Done.
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
echo         !BINDIR!\gamedata\config\vr\  (note: config, not configs)
echo.
echo   4. Start SteamVR, connect Pico, run xrEngine.exe.
echo.
echo   If VR does not start, type vr_status in the game console.
echo.
echo ============================================================
echo.

pause

goto :eof

:clone
rem %1=repo %2=branch %3=target dir
if exist "%~3\.git" (
    echo     skip %~3
) else (
    git clone --branch %~2 --depth 1 https://github.com/%~1 "%~3" >nul 2>&1
    if errorlevel 1 (
        echo     WARN: failed to clone %~1
    ) else (
        echo     ok %~1
    )
)
goto :eof
