#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_05_render.py - патч Layers/xrRenderPC_R4/r4_R_render.cpp
#
# 1) В конце CRender::Render() - отправка готового глаза в SteamVR:
#       vr::RenderFinishEye();
#
# 2) Хелпер CRender::xrVR_ApplyEye(int) - публичная обёртка, которую
#    xrGame вызывает для переключения камеры (см. патч 04).
#
# Раньше здесь была петля по глазам внутри Render() - это ломало
# Calculate()/vis-кэш (второй глаз получал усечённую геометрию).
# Теперь петля - в CLevel::OnRender (патч 04).
# -----------------------------------------------------------------------------
import re, sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(p, s) if False else p.write_text(s, encoding="utf-8")

def main():
    f = ROOT / "ogsr_engine/Layers/xrRenderPC_R4/r4_R_render.cpp"
    s = read(f)

    if "vr::RenderFinishEye" in s:
        print(f"[05] already patched {f}")
        return

    if '#include "../../xrVR/xrVR_render.h"' not in s:
        s = s.replace('#include "stdafx.h"', '#include "stdafx.h"\n#include "../../xrVR/xrVR_render.h"', 1)

    start = s.find("void CRender::Render()")
    if start < 0:
        print("!! [05] CRender::Render not found")
        return

    body_open = s.find("{", start)
    depth = 0
    i = body_open
    end = -1
    while i < len(s):
        c = s[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
        i += 1
    if end < 0:
        print("!! [05] body end not found")
        return

    body = s[body_open:end]  # включая { ... }

    # Вставляем submit в самый конец функции: перед закрывающей }
    # (после всех return-веток; они просто не дойдут до submit - и это ок,
    #  т.к. submit делается только когда сцена реально отрисована)
    new_body = body[:-1] + "\n\n    // ---- VR: отправить отрендеренный глаз в SteamVR ----\n    vr::RenderFinishEye();\n}"

    new_fn = (
        "// ---- VR: переключить камеру на глаз (вызывается из xrGame) ----\n"
        "void CRender::xrVR_ApplyEye(int eye) { vr::RenderApplyEye(eye); }\n\n"
        "void CRender::Render()\n" + new_body
    )

    s = s[:start] + new_fn + s[end + 1:]
    write(f, s)
    print(f"[05] patched {f}")

if __name__ == "__main__":
    main()
