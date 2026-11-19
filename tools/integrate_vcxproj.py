#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# integrate_vcxproj.py - добавляет VR-модуль в проекты Visual Studio OGSR.
#
# Запуск из корня клона OGSR-Engine:
#     python tools/integrate_vcxproj.py .
#
# Что делает:
#   * копирует src/xrVR/*        -> ogsr_engine/xrVR/
#   * копирует third_party/openvr.h -> ogsr_engine/xrVR/openvr/openvr.h
#   * XR_3DA.vcxproj  : добавляет xrVR_api.cpp, xrVR_console.cpp,
#                       xrVR_openvr_null.cpp; include-путь к xrVR/
#   * xrGame.vcxproj  : добавляет xrVR_api.cpp (свой экземпляр),
#                       но xrVR_openvr_null.cpp уже в XR_3DA (линкуется)
#                       -> поэтому xrGame использует ТОЛЬКО заголовок.
#   * xrRender_R4.vcxproj: добавляет xrVR_openvr_r4.cpp, xrVR_render.cpp,
#                       include-путь к xrVR/ и xrVR/openvr/
#
# Все вставки идемпотентны: повторный запуск не дублирует записи.
# -----------------------------------------------------------------------------
import re
import shutil
import sys
from pathlib import Path

# --- Windows: консоль по умолчанию cp1251, русский текст падает -----------
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


ROOT      = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
SRC       = Path(__file__).resolve().parent.parent / "src" / "xrVR"
OPENVR_H  = Path(__file__).resolve().parent.parent / "third_party" / "openvr" / "openvr.h"

ENGINE    = ROOT / "ogsr_engine"
DEST      = ENGINE / "xrVR"
DEST_OVR  = DEST / "openvr"


def copy_sources():
    DEST.mkdir(parents=True, exist_ok=True)
    DEST_OVR.mkdir(parents=True, exist_ok=True)

    copied = []
    for f in SRC.glob("*.h"):
        shutil.copy2(f, DEST / f.name)
        copied.append(f.name)
    for f in SRC.glob("*.cpp"):
        # файл xrVR_openvr.cpp (не _r4) устарел - не копируем
        if f.name == "xrVR_openvr.cpp":
            continue
        shutil.copy2(f, DEST / f.name)
        copied.append(f.name)

    if OPENVR_H.exists():
        shutil.copy2(OPENVR_H, DEST_OVR / "openvr.h")
        copied.append("openvr/openvr.h")

    print(f"[vcx] скопировано в {DEST}: {', '.join(sorted(copied))}")


def patch_vcxproj(path: Path, new_clcompile, new_clinclude, include_dirs):
    if not path.exists():
        print(f"!! [vcx] нет файла {path}")
        return

    s = path.read_text(encoding="utf-8")

    # --- ClCompile ---
    for item in new_clcompile:
        if f'<ClCompile Include="{item}"' in s:
            continue
        m = re.search(r"(\s*<ClCompile Include=\"[^\"]+\"[^>]*/>)", s)
        if m:
            s = s[:m.start()] + f'\n    <ClCompile Include="{item}" />' + s[m.start():]

    # --- ClInclude ---
    for item in new_clinclude:
        if f'<ClInclude Include="{item}"' in s:
            continue
        m = re.search(r"(\s*<ClInclude Include=\"[^\"]+\"[^>]*/>)", s)
        if m:
            s = s[:m.start()] + f'\n    <ClInclude Include="{item}" />' + s[m.start():]

    # --- AdditionalIncludeDirectories ---
    for inc in include_dirs:
        # ищем все объявления и добавляем в каждое, где ещё нет
        def add_inc(match):
            body = match.group(0)
            if inc in body:
                return body
            return body.replace("%(AdditionalIncludeDirectories)",
                                inc + ";%(AdditionalIncludeDirectories)")

        s = re.sub(r"<AdditionalIncludeDirectories>.*?</AdditionalIncludeDirectories>",
                   add_inc, s, flags=re.S)

    path.write_text(s, encoding="utf-8")
    print(f"[vcx] патчен {path}")


def main():
    if not ENGINE.exists():
        print(f"!! [vcx] не найден {ENGINE}. Запустите скрипт из корня клона OGSR-Engine.")
        sys.exit(1)

    copy_sources()

    # XR_3DA: движок (Device/камера/консоль) + нулевая реализация OpenVR
    patch_vcxproj(
        ENGINE / "xr_3da" / "XR_3DA.vcxproj",
        new_clcompile=["xrVR\\xrVR_api.cpp", "xrVR\\xrVR_console.cpp", "xrVR\\xrVR_openvr_null.cpp"],
        new_clinclude=["xrVR\\xrVR.h", "xrVR\\xrVR_math.h", "xrVR\\xrVR_openvr.h", "xrVR\\xrVR_console.h"],
        include_dirs=["$(SolutionDir)ogsr_engine\\xrVR"],
    )

    # xrRender_R4: реальная реализация OpenVR + стерео-рендер
    patch_vcxproj(
        ENGINE / "Layers" / "xrRenderPC_R4" / "xrRender_R4.vcxproj",
        new_clcompile=["..\\..\\xrVR\\xrVR_openvr_r4.cpp", "..\\..\\xrVR\\xrVR_render.cpp"],
        new_clinclude=["..\\..\\xrVR\\xrVR.h", "..\\..\\xrVR\\xrVR_math.h", "..\\..\\xrVR\\xrVR_openvr.h", "..\\..\\xrVR\\xrVR_render.h"],
        include_dirs=["$(SolutionDir)ogsr_engine\\xrVR", "$(SolutionDir)ogsr_engine\\xrVR\\openvr"],
    )

    # xrGame: только заголовки (API линкуется из XR_3DA через xrEngine.lib)
    patch_vcxproj(
        ENGINE / "xrGame" / "xrGame.vcxproj",
        new_clcompile=[],
        new_clinclude=["..\\xrVR\\xrVR.h", "..\\xrVR\\xrVR_math.h"],
        include_dirs=["$(SolutionDir)ogsr_engine\\xrVR"],
    )

    print("\n[vcx] Готово. Соберите Engine.sln (Release|x64).")


if __name__ == "__main__":
    main()
