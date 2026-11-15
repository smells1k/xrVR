#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_01_device.py - патч xr_3da/device.h + xr_3da/device.cpp
#
# ПРИМЕНЯЕТСЯ К: клон OGSR-Engine (ветка main)
#   ogsr_engine/xr_3da/device.h
#   ogsr_engine/xr_3da/device.cpp
#
# Что делает:
#   1) В CRenderDeviceData добавляет поле VR-тайминга кадра
#   2) В CRenderDevice::FrameMove() в самое начало ставит vr::OnFrameBegin()
#   3) В CRenderDevice::End() в самый конец ставит vr::OnFrameEnd()
#   4) В CRenderDevice::_Create() / Destroy() - vr::OnDeviceCreate/Destroy
#   5) В CRenderDevice::Destroy() - vr::Shutdown()
# -----------------------------------------------------------------------------
import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

def read(p):
    return p.read_text(encoding="utf-8-sig" if p.exists() else "utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

def ensure_include(text, inc, after_marker):
    """Добавляет #include после нужной строки, если его еще нет."""
    if inc in text:
        return text, False
    idx = text.find(after_marker)
    if idx < 0:
        return text, False
    end = text.find("\n", idx) + 1
    return text[:end] + inc + "\n" + text[end:], True

def patch_device_cpp(root: Path):
    f = root / "ogsr_engine/xr_3da/device.cpp"
    s = read(f)

    if "../xrVR/xrVR.h" not in s:
        s, _ = ensure_include(s, '#include "../xrVR/xrVR.h"', '#include "xr_input.h"')

    # заголовок стерео-рендера: нужен для RenderUpdateScreen()
    if "../xrVR/xrVR_render.h" not in s:
        s, _ = ensure_include(s, '#include "../xrVR/xrVR_render.h"',
                              '#include "../xrVR/xrVR.h"')

    # --- 1. Начало кадра: в FrameMove()
    if "vr::OnFrameBegin()" not in s:
        anchor = "void CRenderDevice::FrameMove()\n{\n"
        if anchor not in s:
            anchor = "void CRenderDevice::FrameMove()\r\n{\r\n"
        if anchor in s:
            eol = "\r\n" if "\r\n" in anchor else "\n"
            ins = ("    // --- VR: снять позу HMD в начале кадра ---" + eol +
                   "    vr::OnFrameBegin();" + eol +
                   eol)
            s = s.replace(anchor, anchor + ins, 1)
        else:
            print("!! [01] FrameMove anchor not found")

    # --- 2. Конец кадра: в End() после ::Render->End();
    if "vr::OnFrameEnd()" not in s:
        anchor = "    ::Render->End();\n"
        if anchor not in s:
            anchor = "    ::Render->End();\r\n"
        if anchor in s:
            eol = "\r\n" if "\r\n" in anchor else "\n"
            ins = ("    // --- VR: завершить кадр (present в SteamVR уже сделан Submit'ом) ---" + eol +
                   "    vr::OnFrameEnd();" + eol +
                   "    // VR-экран: backbuffer -> overlay (меню/ПДА/загрузка)" + eol +
                   "    vr::RenderUpdateScreen();" + eol)
            s = s.replace(anchor, anchor + ins, 1)
        else:
            print("!! [01] Render->End anchor not found")

    write(f, s)
    print(f"[01] patched {f}")

    # --- 3. Создание/уничтожение device (отдельные файлы OGSR)
    fc = root / "ogsr_engine/xr_3da/Device_create.cpp"
    sc = read(fc)
    if "vr::OnDeviceCreate()" not in sc:
        if '#include "../xrVR/xrVR.h"' not in sc:
            sc = '#include "../xrVR/xrVR.h"\n' + sc
        m = re.search(r"(void CRenderDevice::_Create\(\)\s*\r?\n\{\s*\r?\n)", sc)
        if m:
            sc = sc[:m.end()] + "    // --- VR ---\n    vr::OnDeviceCreate();\n\n" + sc[m.end():]
        else:
            print("!! [01] _Create anchor not found in Device_create.cpp")
    write(fc, sc)
    print(f"[01] patched {fc}")

    fd = root / "ogsr_engine/xr_3da/Device_destroy.cpp"
    sd = read(fd)
    if "vr::OnDeviceDestroy()" not in sd:
        if '#include "../xrVR/xrVR.h"' not in sd:
            sd = '#include "../xrVR/xrVR.h"\n' + sd 
        m = re.search(r"(void CRenderDevice::_Destroy\(BOOL bKeepTextures\)\s*\r?\n\{\s*\r?\n)", sd)
        if m:
            sd = sd[:m.end()] + "    // --- VR ---\n    vr::OnDeviceDestroy();\n\n" + sd[m.end():]
        else:
            print("!! [01] _Destroy anchor not found in Device_destroy.cpp")
    write(fd, sd)
    print(f"[01] patched {fd}")

if __name__ == "__main__":
    patch_device_cpp(ROOT)
