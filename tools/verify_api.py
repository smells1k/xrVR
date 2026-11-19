#!/usr/bin/env python3
"""verify_api.py - проверяет целостность API модуля xrVR.

Ловит самые частые ошибки сборки:
  1. функция вызывается из патча, но не объявлена в xrVR.h/xrVR_render.h
  2. функция объявлена в .h, но не определена в .cpp (ошибка линковки LNK2019)
  3. функция определена, но не объявлена (ошибка C2065/не видна снаружи)

Запуск: python tools/verify_api.py
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
SRC = ROOT / "src" / "xrVR"

HEADERS = ["xrVR.h", "xrVR_render.h", "xrVR_openvr.h", "xrVR_console.h", "xrVR_math.h"]
SOURCES = ["xrVR_api.cpp", "xrVR_console.cpp", "xrVR_openvr_null.cpp",
           "xrVR_openvr_r4.cpp", "xrVR_render.cpp"]

# Функции, которые вызываются из патчей движка (внешние точки входа).
PATCH_CALLS = {
    "xrVR.h": [
        "Init", "Shutdown", "LoadConfig", "OnDeviceCreate", "OnDeviceDestroy",
        "OnLevelLoad", "OnLevelUnload", "OnFrameBegin", "OnFrameEnd",
        "GetHeadPose", "GetHeadLook", "Recenter", "ConsumeRecenter",
        "AdjustFireParams", "GetStatusString", "LogLine",
        "IsActive", "IsInitialized", "GetConfig", "GetState",
    ],
    "xrVR_render.h": [
        "RenderInit", "RenderShutdown", "RenderIsActive", "RenderBeginStereo",
        "RenderApplyEye", "RenderFinishEye", "RenderEndStereo", "RenderInStereo",
        "RenderUpdateScreen", "RenderReleaseScreenTexture",
    ],
    "xrVR_openvr.h": [
        "xrVR_BindOpenVRImpl", "xrVR_GetOpenVRImpl", "OvrGetImpl",
        "OpenVR_Init", "OpenVR_Shutdown", "OpenVR_BeginFrame", "OpenVR_EndFrame",
        "OpenVR_GetRawHeadPose", "OpenVR_GetGameHeadPose", "OpenVR_GetEyeToHead",
        "OpenVR_GetEyeProjectionParams", "OpenVR_GetRecommendedRenderSize",
        "OpenVR_Submit", "OpenVR_ShowScreen", "OpenVR_SetScreenTexture",
        "OpenVR_GetHMDRefreshRate", "OpenVR_GetHMDName", "OpenVR_GetRuntimeName",
    ],
}

ok = 0
bad = 0


def check(cond, msg):
    global ok, bad
    if cond:
        ok += 1
    else:
        bad += 1
        print(f"  FAIL: {msg}")


print("[1] Файлы на месте")
for f in HEADERS + SOURCES:
    check((SRC / f).exists(), f"нет файла {f}")

print("[2] Точки входа объявлены в заголовках")
header_text = {h: (SRC / h).read_text(encoding="utf-8") for h in HEADERS if (SRC / h).exists()}
for hdr, funcs in PATCH_CALLS.items():
    text = header_text.get(hdr, "")
    for fn in funcs:
        # объявление: имя + '(' где-то в заголовке
        found = re.search(rf"\b{re.escape(fn)}\s*\(", text) is not None
        check(found, f"{fn}() не объявлена в {hdr}")

print("[3] Объявления определены в .cpp")
src_text = ""
for s in SOURCES:
    p = SRC / s
    if p.exists():
        src_text += p.read_text(encoding="utf-8") + "\n"

for hdr, funcs in PATCH_CALLS.items():
    text = header_text.get(hdr, "")
    for fn in funcs:
        if not re.search(rf"\b{re.escape(fn)}\s*\(", text):
            continue  # уже провалено выше
        # определение в .cpp: имя( ... ) {  (не точка с запятой сразу)
        defined = re.search(rf"^\s*(?:[\w:<>*&\s]+?\s)?\b{re.escape(fn)}\s*\([^;]*?\)\s*$",
                            src_text, re.M) is not None
        if not defined:
            # мягкий вариант - имя( где-либо, затем '{' в пределах 3 строк
            defined = re.search(rf"\b{re.escape(fn)}\s*\([^;{{}}]*\)\s*\n?\s*\{{", src_text) is not None
        check(defined, f"{fn}() объявлена в {hdr}, но не найдена реализация в .cpp")

print("[4] Реализация OpenVR не дублируется")
dupes = 0
impl_files = [s for s in SOURCES if "openvr_r4" in s or s == "xrVR_openvr.cpp"]
for s in impl_files:
    p = SRC / s
    if not p.exists():
        continue
    txt = p.read_text(encoding="utf-8")
    if "VR_InitInternal" in txt or "VR_Init(" in txt or "g_system->" in txt:
        dupes += 1
check(dupes <= 1, f"найдено {dupes} реализаций OpenVR (ожидается 1: xrVR_openvr_r4.cpp)")
check(not (SRC / "xrVR_openvr.cpp").exists(),
      "xrVR_openvr.cpp всё ещё существует - удалите (дубль xrVR_openvr_r4.cpp)")

print("[5] PCH подключается голым stdafx.h (требование MSVC /Yu)")
# MSVC ищет PCH ТОЛЬКО по точному имени из /Yu. 'stdafx.h' -> ок,
# '../xr_3da/stdafx.h' -> C1010 'unexpected end of file'.
for _f in sorted(SRC.glob("*.cpp")):
    _t = _f.read_text(encoding="utf-8")
    _sig = [l for l in _t.splitlines() if l.strip() and not l.strip().startswith("//")]
    check(_sig and _sig[0].strip().startswith('#include "stdafx.h"'),
          f"{_f.name}: первым оператором должен быть #include \"stdafx.h\" "
          f"(сейчас: {_sig[0].strip()[:40] if _sig else 'пусто'}) -> MSVC C1010")

print("[6] openvr.h подключён только в реализациях")
for s in SOURCES:
    p = SRC / s
    if not p.exists():
        continue
    txt = p.read_text(encoding="utf-8")
    has_openvr = '#include "openvr.h"' in txt
    is_impl = ("openvr_r4" in s)
    is_null = ("openvr_null" in s)
    if is_impl:
        check(has_openvr, f"{s}: должен включать openvr.h")
    elif s in ("xrVR_api.cpp", "xrVR_console.cpp", "xrVR_render.cpp") or is_null:
        check(not has_openvr, f"{s}: НЕ должен включать openvr.h (он есть только в R4)")

print("[7] Конфиг читается из gamedata/configs/vr/vr_config.ltx")
api_cpp = (SRC / "xrVR_api.cpp")
if api_cpp.exists():
    t = api_cpp.read_text(encoding="utf-8")
    # pSettings - это только configs/system.ltx (x_ray.cpp:44)
    check('FS.update_path' in t and "vr_config.ltx" in t,
          "LoadConfig не читает vr_config.ltx через FS.update_path")
    check('section_exist("vr")' in t and "vrIni" in t,
          "LoadConfig не использует собственный CInifile для [vr]")
    # bLoad должен быть TRUE, иначе файл не парсится
    check("TRUE, TRUE, FALSE" in t,
          "CInifile создаётся с bLoad=FALSE - файл не будет прочитан")

print("[8] HUD (руки/оружие) пересчитывается под глаз")
rcpp = (SRC / "xrVR_render.cpp")
if rcpp.exists():
    t = rcpp.read_text(encoding="utf-8")
    check("mProject_hud" in t, "RenderApplyEye не пересчитывает HUD-проекцию")
    check("psHUD_FOV" in t, "не используется psHUD_FOV - HUD разъедется по глазам")
    check("mView_hud_old" in t, "не обновлены *_hud_old - HUD возьмёт матрицы прошлого кадра")

print("[9] Нет обращений к приватным полям движка")
# CTexture::pSurface - private (SH_Texture.h:120). Публичный аксессор - surface_get().
render_cpp = (SRC / "xrVR_render.cpp")
if render_cpp.exists():
    txt = render_cpp.read_text(encoding="utf-8")
    check("->pSurface" not in txt and ".pSurface" not in txt,
          "xrVR_render.cpp обращается к приватному CTexture::pSurface (нужен surface_get())")
    if "surface_get()" in txt:
        check(True, "")
print("[10] install.bat запускается на Windows")
bat = ROOT / "install.bat"
if bat.exists():
    raw = bat.read_bytes()
    check(raw[:3] != b'\xef\xbb\xbf',
          "install.bat: UTF-8 BOM ломает первую строку (cmd читает 'n++@echo off')")
    check(b'\r\n' in raw, "install.bat: нужны CRLF, иначе cmd не разберёт файл")
    nonascii = sum(1 for b in raw if b > 127)
    check(nonascii == 0, f"install.bat: {nonascii} не-ASCII байт, cmd споткнётся")

print("[11] В коде нет кириллицы (MSVC C4566 при cp1252)")
import re as _re
_bad = []
for _f in sorted(SRC.glob("*")):
    if _f.suffix not in (".cpp", ".h"):
        continue
    for _i, _l in enumerate(_f.read_text(encoding="utf-8").splitlines(), 1):
        if _re.search(r"[\u0400-\u04FF]", _re.sub(r"//.*$", "", _l)):
            _bad.append(f"{_f.name}:{_i}")
check(not _bad, "кириллица в коде: " + ", ".join(_bad[:5]))

print("[12] Препроцессор сбалансирован ПО ПОРЯДКУ (#if/#endif)")
for _f in sorted(SRC.glob("*.cpp")):
    _stack, _err = [], None
    for _i, _l in enumerate(_f.read_text(encoding="utf-8").splitlines(), 1):
        _m = _re.match(r"\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b", _l)
        if not _m:
            continue
        _k = _m.group(1)
        if _k in ("if", "ifdef", "ifndef"):
            _stack.append(_i)
        elif _k == "endif":
            if not _stack:
                _err = f"лишний #endif на строке {_i} (MSVC C1020)"; break
            _stack.pop()
        elif not _stack:
            _err = f"#{_k} на строке {_i} без #if"; break
    if not _err and _stack:
        _err = f"незакрытые #if на строках {_stack} (MSVC C1014)"
    check(_err is None, f"{_f.name}: {_err}")

print("[13] PCH идёт ПЕРВЫМ оператором (до любого #if)")
for _f in sorted(SRC.glob("*.cpp")):
    _sig = [l for l in _f.read_text(encoding="utf-8").splitlines()
            if l.strip() and not l.strip().startswith("//")]
    if not _sig or '#include "stdafx.h"' not in _f.read_text(encoding="utf-8"):
        continue
    check(_sig[0].strip().startswith('#include "stdafx.h"'),
          f"{_f.name}: первый оператор '{_sig[0].strip()[:30]}', а не #include \"stdafx.h\""
          " (при /Yu MSVC требует PCH раньше любого #if)")

print("[14] Нет предобъявлений typedef через struct")
h_txt = (SRC / "xrVR.h")
if h_txt.exists():
    t = h_txt.read_text(encoding="utf-8")
    check("struct Fvector;" not in t, "xrVR.h: 'struct Fvector;' - Fvector это typedef (MSVC: using typedef-name after struct)")
    check("struct Fmatrix;" not in t, "xrVR.h: 'struct Fmatrix;' - Fmatrix это typedef")

print()
print(f"ИТОГО: {ok} проверок пройдено, {bad} провалено")
sys.exit(1 if bad else 0)
