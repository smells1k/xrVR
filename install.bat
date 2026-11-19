@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion
title xrVR - установка и сборка

REM ===========================================================================
REM  xrVR - установка мода и сборка движка одной кнопкой.
REM
REM  Что делает скрипт:
REM    1. Находит Visual Studio (vswhere, потом перебор путей)
REM    2. Проверяет Python
REM    3. Клонирует OGSR-Engine, если папки ещё нет
REM    4. Применяет патчи VR-мода
REM    5. Собирает Release|x64 через MSBuild
REM    6. Собирает папку bin\ и говорит, что положить рядом с exe
REM
REM  Положить этот файл в корень репозитория xrVR (рядом с tools\).
REM ===========================================================================

set "ROOT=%~dp0"
cd /d "%ROOT%"

echo.
echo ============================================================
echo   xrVR. VR-мод для S.T.A.L.K.E.R. ТЧ на OGSR Engine
echo ============================================================
echo.

REM --- путь к движку ---------------------------------------------------------
if "%~1"=="" (
    set "ENGINE=%ROOT%OGSR-Engine"
) else (
    set "ENGINE=%~1"
)
echo Движок будет здесь: !ENGINE!
echo.

REM ===========================================================================
REM 1. VISUAL STUDIO
REM ===========================================================================
echo [1/6] Ищу Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD="

if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
        set "MSBUILD=%%i"
    )
)

if not defined MSBUILD (
    echo    vswhere не нашёл. Перебираю стандартные пути...
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
    echo    !!! Visual Studio не найдена.
    echo    Установи Visual Studio 2022 (Community хватит) с компонентом
    echo    "Desktop development with C++" и запусти этот файл снова.
    echo.
    pause
    exit /b 1
)
echo    OK: !MSBUILD!
echo.

REM ===========================================================================
REM 2. PYTHON
REM ===========================================================================
echo [2/6] Проверяю Python...

set "PY="
where python >nul 2>&1 && set "PY=python"
if not defined PY ( where py >nul 2>&1 && set "PY=py" )

if not defined PY (
    echo.
    echo    !!! Python не найден.
    echo    Поставь Python 3.8+ с python.org, при установке отметь
    echo    "Add python.exe to PATH". Потом запусти файл снова.
    echo.
    pause
    exit /b 1
)
for /f "tokens=*" %%v in ('%PY% --version 2^>^&1') do echo    OK: %%v
echo.

REM ===========================================================================
REM 3. ДВИЖОК
REM ===========================================================================
echo [3/6] Движок OGSR...

if exist "!ENGINE!\Engine.sln" (
    echo    Уже есть: !ENGINE!
) else (
    echo    Клонирую OGSR-Engine в !ENGINE!
    echo    Это ~139 МБ, подожди...
    git clone --depth 1 --filter=blob:none https://github.com/OGSR/OGSR-Engine.git "!ENGINE!"
    if errorlevel 1 (
        echo.
        echo    !!! Не смог склонировать. Проверь, что установлен git
        echo    и есть интернет. Либо склонируй сам и передай путь:
        echo        install.bat C:\путь\к\OGSR-Engine
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

REM ===========================================================================
REM 4. ПАТЧИ МОДА
REM ===========================================================================
echo [4/6] Применяю патчи VR-мода...

%PY% tools\apply_patches.py "!ENGINE!"
if errorlevel 1 (
    echo.
    echo    !!! Патчи не применились. Смотри текст выше.
    echo    Если написано "anchor not found" - версия движка не та.
    echo.
    pause
    exit /b 1
)
echo.

REM ===========================================================================
REM 5. СБОРКА
REM ===========================================================================
echo [5/6] Собираю Release^|x64...
echo    Первый раз это 15-40 минут. Не закрывай окно.
echo.

"!MSBUILD!" "!ENGINE!\Engine.sln" /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 (
    echo.
    echo    !!! Сборка упала. Смотри ошибки выше.
    echo    Частые причины:
    echo      - не установлен Windows SDK  ^(ставь через Visual Studio Installer^)
    echo      - не хватает компонента "Desktop development with C++"
    echo      - путь содержит кириллицу ^(перенеси в C:\OGSR^)
    echo.
    pause
    exit /b 1
)
echo.

REM ===========================================================================
REM 6. РЕЗУЛЬТАТ
REM ===========================================================================
echo [6/6] Готово.
echo.

set "BINDIR=!ENGINE!\bin"
if not exist "!BINDIR!\xrEngine.exe" (
    for /d %%d in ("!ENGINE!\*") do (
        if exist "%%d\xrEngine.exe" set "BINDIR=%%d"
    )
)

if exist "!BINDIR!\xrEngine.exe" (
    echo    Собрано: !BINDIR!\xrEngine.exe
) else (
    echo    exe не найден. Поищи его сам в !ENGINE!
)

echo.
echo ============================================================
echo   ОСТАЛОСЬ СДЕЛАТЬ РУКАМИ ^(3 минуты^)
echo ============================================================
echo.
echo   1. Скопируй openvr_api.dll из SteamVR:
echo      Steam\steamapps\common\SteamVR\bin\win64\openvr_api.dll
echo      в папку:
echo      !BINDIR!
echo.
echo   2. Скопируй туда же gamedata\ из лицензионной ТЧ 1.0006
echo      ^(configs, scripts, meshes, textures, sounds, levels^).
echo.
echo   3. Скопируй gamedata\configs\vr\vr_config.ltx из этого
echo      репозитория в:
echo      !BINDIR!\gamedata\configs\vr\
echo.
echo   4. Запусти SteamVR, подключи Pico, запусти xrEngine.exe.
echo.
echo   Если VR не включился - в консоли игры ^(~^) набери vr_status
echo.
echo ============================================================
echo.

pause
