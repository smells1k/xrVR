#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_03_weapon.py - патч xrGame/Actor_Weapon.cpp
#
# CActor::g_fireParams: при активном VR направление выстрела берётся
# из взгляда HMD (ствол "смотрит" туда, куда смотрит игрок).
# Позиция выстрела остаётся у оружия - так сохраняется баллистика и
# визуал трассеров из ствола.
# -----------------------------------------------------------------------------
import sys
from pathlib import Path

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
    f = ROOT / "ogsr_engine/xrGame/Actor_Weapon.cpp"
    s = read(f)

    if "#include \"xrVR.h\"" not in s:
        s = s.replace('#include "stdafx.h"', '#include "stdafx.h"\n#include "xrVR.h"', 1)

    if "vr::AdjustFireParams" not in s:
        anchor = "void CActor::g_fireParams(CHudItem* pHudItem, Fvector& fire_pos, Fvector& fire_dir, const bool for_cursor)\n{\n"
        if anchor not in s:
            print("!! [03] g_fireParams anchor not found")
        else:
            block = (
                "    // --- VR: направление выстрела по взгляду HMD ---\n"
                "    // (только для не-курсорной стрельбы и только в VR)\n"
                "    if (!for_cursor && vr::IsActive())\n"
                "    {\n"
                "        fire_dir = Cameras().Direction();\n"
                "        fire_pos = Cameras().Position();\n"
                "        vr::AdjustFireParams(fire_pos, fire_dir);\n"
                "        return;\n"
                "    }\n\n"
            )
            s = s.replace(anchor, anchor + block, 1)
            write(f, s)
            print(f"[03] patched {f}")

if __name__ == "__main__":
    main()
