#!/usr/bin/env python3
"""verify_integration.py - проверяет, что после применения патчей и
интеграции в vcxproj модуль действительно сможет собраться.

Проверяет:
  1. Все относительные #include в src/xrVR/*.cpp разрешаются в дереве движка
     (после копирования в ogsr_engine/xrVR/).
  2. Все функции vr::*, вызываемые из пропатченного кода, объявлены в
     подключённом туда заголовке.
  3. Все движковые символы, которые использует модуль, реально существуют
     (grep по исходникам OGSR).
  4. Каждый .cpp из модуля присутствует ровно в одном vcxproj.

Запуск: python tools/verify_integration.py [путь_к_клону]
"""
import re
import sys
from pathlib import Path

# --- Windows: консоль по умолчанию cp1251, русский текст падает -----------
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/build_check")
ENG = ENGINE_ROOT / "ogsr_engine"
XRVR = ENG / "xrVR"

ok = bad = 0


def check(cond, msg):
    global ok, bad
    if cond:
        ok += 1
    else:
        bad += 1
        print(f"  FAIL: {msg}")


if not ENG.exists():
    print(f"!! нет {ENG}; запустите: python tools/apply_patches.py <клон>")
    sys.exit(1)
if not XRVR.exists():
    print(f"!! нет {XRVR}; запустите: python tools/integrate_vcxproj.py <клон>")
    sys.exit(1)

print("[1] Относительные #include модуля разрешаются")
inc_re = re.compile(r'#include\s+"([^"]+)"')
n_inc = 0
for cpp in sorted(XRVR.glob("*.cpp")):
    for line_no, line in enumerate(cpp.read_text(encoding="utf-8").splitlines(), 1):
        m = inc_re.search(line)
        if not m:
            continue
        inc = m.group(1)
        if inc.startswith("<"):
            continue
        target = (cpp.parent / inc).resolve()
        n_inc += 1
        # openvr.h лежит в xrVR/openvr/ (include-путь из vcxproj)
        # stdafx.h приходит из проекта-хозяина (xrRenderPC_R4 / xr_3da):
        # MSVC находит его через /I. проекта, поэтому локально файла нет.
        if inc == "stdafx.h":
            continue
        alt = (XRVR / "openvr" / inc).resolve()
        check(target.exists() or alt.exists(),
              f"{cpp.name}:{line_no} не найден #include \"{inc}\" (ни {target}, ни {alt})")
print(f"  проверено {n_inc} включений")

print("[2] Вызовы vr::* в пропатченном коде объявлены")
patched = {
    "xrGame/CameraFirstEye.cpp": ["GetHeadLook", "IsActive", "ConsumeRecenter"],
    "xrGame/Actor_Weapon.cpp": ["IsActive", "AdjustFireParams"],
    "xrGame/Level.cpp": ["RenderBeginStereo", "RenderApplyEye", "RenderEndStereo"],
    "Layers/xrRenderPC_R4/r4_R_render.cpp": ["RenderFinishEye"],
    "Layers/xrRenderPC_R4/r4.cpp": ["xrVR_BindOpenVRImpl", "OvrGetImpl"],
    "xr_3da/device.cpp": ["OnFrameBegin", "OnFrameEnd"],
    "xr_3da/Device_create.cpp": ["OnDeviceCreate"],
    "xr_3da/Device_destroy.cpp": ["OnDeviceDestroy"],
}
decl_text = ""
for h in XRVR.glob("*.h"):
    decl_text += h.read_text(encoding="utf-8") + "\n"

for f, funcs in patched.items():
    p = ENG / f
    check(p.exists(), f"нет пропатченного файла {f}")
    if not p.exists():
        continue
    txt = p.read_text(encoding="utf-8")
    includes_vr = re.search(r'#include\s+"[^"]*xrVR', txt) is not None
    check(includes_vr, f"{f}: нет #include на заголовок xrVR")
    for fn in funcs:
        check(re.search(rf"\b{re.escape(fn)}\s*\(", txt) is not None,
              f"{f}: вызов {fn}() отсутствует")
        check(re.search(rf"\b{re.escape(fn)}\s*\(", decl_text) is not None,
              f"{f}: {fn}() не объявлена ни в одном заголовке xrVR")

print("[3] Движковые символы, используемые модулем, существуют")
engine_syms = {
    "RImplementation": "Layers/xrRenderPC_R4/r4.cpp",
    "SetupDisplayBackbuffer": "Layers/xrRenderPC_R4/r4.h",
    "SetCacheXform": "Layers/xrRenderPC_R4/r4.h",
    "CreateFromMatrix": "xrCDB/Frustum.h",
    "build_projection_HAT": "xrCore/_matrix.h",
    "build_camera_dir": "xrCore/_matrix.h",
    "vCameraPosition": "xr_3da/device.h",
    "mFullTransform": "xr_3da/device.h",
    "psHUD_FOV_def": "xr_3da/xr_ioc_cmd.cpp",
    "rt_base": "Layers/xrRenderPC_R4/r4_rendertarget.h",
}
for sym, where in engine_syms.items():
    files = list(ENG.rglob(Path(where).name))
    found = any(sym in f.read_text(encoding="utf-8", errors="ignore")
                for f in files if f.is_file())
    check(found, f"символ {sym} не найден в {where}")

print("[4] Каждый .cpp модуля ровно в одном vcxproj")
projs = {
    "XR_3DA": ENG / "xr_3da" / "XR_3DA.vcxproj",
    "xrRender_R4": ENG / "Layers" / "xrRenderPC_R4" / "xrRender_R4.vcxproj",
    "xrGame": ENG / "xrGame" / "xrGame.vcxproj",
}
proj_text = {k: v.read_text(encoding="utf-8") for k, v in projs.items() if v.exists()}
for cpp in sorted(XRVR.glob("*.cpp")):
    hits = [k for k, t in proj_text.items() if cpp.name in t]
    check(len(hits) == 1, f"{cpp.name} найден в {len(hits)} проектах: {hits}")

print("[5] Обязательные файлы в проектах")
check("xrVR_openvr_r4.cpp" in proj_text.get("xrRender_R4", ""), "R4: нет xrVR_openvr_r4.cpp")
check("xrVR_render.cpp" in proj_text.get("xrRender_R4", ""), "R4: нет xrVR_render.cpp")
check("xrVR_openvr_null.cpp" in proj_text.get("XR_3DA", ""), "XR_3DA: нет xrVR_openvr_null.cpp")
check("xrVR_console.cpp" in proj_text.get("XR_3DA", ""), "XR_3DA: нет xrVR_console.cpp")
check("openvr" in proj_text.get("xrRender_R4", ""), "R4: нет include-пути к openvr/")

print()
print(f"ИТОГО: {ok} проверок пройдено, {bad} провалено")
sys.exit(1 if bad else 0)
