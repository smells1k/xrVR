#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_06_bind.py - патч Layers/xrRenderPC_R4/r4.cpp
#
# При создании рендера (CRender::create) привязываем таблицу реальной
# реализации OpenVR к движку: vr::xrVR_BindOpenVRImpl(vr::OvrGetImpl()).
# -----------------------------------------------------------------------------
import re, sys
from pathlib import Path
import sys

# --- Windows: консоль по умолчанию cp1251, русский текст падает -----------
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

def main():
    f = ROOT / "ogsr_engine/Layers/xrRenderPC_R4/r4.cpp"
    s = read(f)

    if "xrVR_BindOpenVRImpl" in s:
        print(f"[06] already patched {f}")
        return

    if '#include "../../xrVR/xrVR_openvr.h"' not in s:
        s = s.replace('#include "stdafx.h"', '#include "stdafx.h"\n#include "../../xrVR/xrVR_openvr.h"', 1)

    m = re.search(r"void CRender::create\(\)\s*\r?\n\{\s*\r?\n", s)
    if not m:
        print("!! [06] CRender::create anchor not found")
        return

    s = (s[:m.end()]
         + "    // ---- VR: привязка реальной реализации OpenVR ----\n"
           "    vr::xrVR_BindOpenVRImpl(vr::OvrGetImpl());\n\n"
         + s[m.end():])
    write(f, s)
    print(f"[06] patched {f}")

if __name__ == "__main__":
    main()
