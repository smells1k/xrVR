#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_04_level.py - патч xrGame/Level.cpp
#
# CLevel::OnRender():
#
#   Обычно:   Render->Calculate(); Render->Render();
#
#   В VR:     для каждого глаза:
#                 vr::RenderApplyEye(e)   - камера/проекция глаза
#                 Render->Calculate()     - видимость/освещение ПОД ЭТОТ глаз
#                 Render->Render()        - отрисовка глаза
#                 Submit -> SteamVR       (внутри CRender::Render)
#             vr::RenderEndStereo()       - вернуть камеру/размер/backbuffer
#
#   БЕЗ этого "Calculate" выполнился бы один раз для средней камеры,
#   и второй глаз получил бы culling/lighting от первого => артефакты.
# -----------------------------------------------------------------------------
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

def main():
    f = ROOT / "ogsr_engine/xrGame/Level.cpp"
    s = read(f)

    if '#include "xrVR.h"' not in s:
        s = s.replace('#include "stdafx.h"', '#include "stdafx.h"\n#include "xrVR.h"', 1)

    if "vr::RenderApplyEye" in s:
        print(f"[04] already patched {f}")
        write(f, s)
        return

    anchor = "    Render->Calculate();\n    Render->Render();\n"
    if anchor not in s:
        anchor = "    Render->Calculate();\r\n    Render->Render();\r\n"
    if anchor not in s:
        print("!! [04] Calculate/Render anchor not found")
        write(f, s)
        return

    eol = "\r\n" if "\r\n" in anchor else "\n"
    block = (
        "    // ---- VR: стерео-проход ----" + eol +
        "    if (vr::RenderBeginStereo() == 2)" + eol +
        "    {" + eol +
        "        for (int vr_eye = 0; vr_eye < 2; ++vr_eye)" + eol +
        "        {" + eol +
        "            vr::RenderApplyEye(vr_eye);" + eol +
        "            Render->Calculate();" + eol +
        "            Render->Render();" + eol +
        "        }" + eol +
        "        vr::RenderEndStereo();" + eol +
        "    }" + eol +
        "    else" + eol +
        "    {" + eol +
        "        Render->Calculate();" + eol +
        "        Render->Render();" + eol +
        "    }" + eol
    )
    s = s.replace(anchor, block, 1)
    write(f, s)
    print(f"[04] patched {f}")

if __name__ == "__main__":
    main()
