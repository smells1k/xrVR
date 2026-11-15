#!/usr/bin/env python3
"""apply_patches.py - применяет ВСЁ: патчи движка + интеграцию в vcxproj.

Использование:
    python tools/apply_patches.py <путь_к_клону_OGSR-Engine>

Порядок:
    1. patch_01..patch_06  - правки .cpp/.h движка
    2. integrate_vcxproj   - копирование модуля + правка .vcxproj

Скрипт идемпотентен: повторный запуск ничего не испортит.
Перед запуском сделайте коммит, чтобы `git checkout .` мог откатить.
"""
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PATCHES = sorted(TOOLS.glob("patch_0*.py"))

if len(sys.argv) < 2:
    print(__doc__)
    print("Найденные патчи:")
    for p in PATCHES:
        print("   ", p.name)
    sys.exit(2)

target = Path(sys.argv[1]).resolve()
if not (target / "ogsr_engine").exists():
    print(f"!! {target} не похож на клон OGSR-Engine (нет ogsr_engine/)")
    sys.exit(1)

print("=" * 60)
print(f"Цель: {target}")
print("=" * 60)

failed = False
for p in PATCHES:
    print(f"\n--- {p.name} ---")
    r = subprocess.run([sys.executable, str(p), str(target)],
                       capture_output=True, text=True)
    out = (r.stdout + r.stderr).strip()
    if out:
        print(out)
    if r.returncode != 0:
        failed = True
        print(f"!! ПАТЧ {p.name} ЗАВЕРШИЛСЯ С ОШИБКОЙ")

print("\n--- integrate_vcxproj.py ---")
r = subprocess.run([sys.executable, str(TOOLS / "integrate_vcxproj.py"), str(target)],
                   capture_output=True, text=True)
print((r.stdout + r.stderr).strip())
if r.returncode != 0:
    failed = True

print("\n--- verify_api.py ---")
r = subprocess.run([sys.executable, str(TOOLS / "verify_api.py")],
                   capture_output=True, text=True)
print((r.stdout + r.stderr).strip()[-1500:])

print(f"\n--- verify_integration.py {target} ---")
r = subprocess.run([sys.executable, str(TOOLS / "verify_integration.py"), str(target)],
                   capture_output=True, text=True)
print((r.stdout + r.stderr).strip()[-2000:])
if r.returncode != 0:
    failed = True

print("\n" + "=" * 60)
if failed:
    print("РЕЗУЛЬТАТ: ЕСТЬ ОШИБКИ - смотрите вывод выше")
    sys.exit(1)
print("РЕЗУЛЬТАТ: ВСЁ ПРИМЕНЕНО")
print("Дальше: откройте Engine.sln в Visual Studio, соберите Release|x64.")
print("=" * 60)
