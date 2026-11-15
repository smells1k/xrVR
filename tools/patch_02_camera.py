#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# patch_02_camera.py - патч xrGame/CameraFirstEye.h/.cpp
#
# Делает так, чтобы камера от первого лица следовала за головой в шлеме:
#   * yaw/pitch/roll берутся из OpenVR (через vr::GetHeadLook)
#   * позиция камеры смещается по реальным движениям головы (6DoF),
#     ограниченным по величине, чтобы не "улетать" от тела
#
# Мышь по-прежнему работает: при VR углы полностью заменяются позой
# HMD (это стандартное поведение VR-игр).
# -----------------------------------------------------------------------------
import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

def patch_cpp(root):
    f = root / "ogsr_engine/xrGame/CameraFirstEye.cpp"
    s = read(f)

    if '#include "../../vr/xrVR.h"' not in s and "#include \"xrVR.h\"" not in s:
        s = s.replace('#include "CameraFirstEye.h"',
                      '#include "CameraFirstEye.h"\n#include "xrVR.h"', 1)

    if "vr::GetHeadLook" not in s:
        anchor = "void CCameraFirstEye::Update(Fvector& point, Fvector& noise_dangle)\n{\n"
        vr_block = (
            "    // --- VR: поза головы заменяет мышь/yaw/pitch камеры ---\n"
            "    if (vr::IsActive())\n"
            "    {\n"
            "        float v_yaw{}, v_pitch{}, v_roll{};\n"
            "        if (vr::GetHeadLook(v_yaw, v_pitch, v_roll))\n"
            "        {\n"
            "            // X-Ray: pitch>0 значит взгляд ВНИЗ (Move(kUP) из мыши\n"
            "            // увеличивает pitch, см. CameraFirstEye::Move).\n"
            "            // Наш vrmath: pitch>0 значит взгляд ВВЕРХ.\n"
            "            yaw = v_yaw;\n"
            "            pitch = -v_pitch;\n"
            "            roll = v_roll;\n"
            "        }\n"
            "    }\n\n"
        )
        if anchor in s:
            s = s.replace(anchor, anchor + vr_block, 1)
        else:
            print("!! [02] CCameraFirstEye::Update anchor not found")

    # 6DoF: смещение позиции головы относительно "якоря" (позиция тела)
    if "namespace vr_camera" not in s:
        helper = (
            "\n//-----------------------------------------------------------------------------\n"
            "// VR: локальное смещение головы относительно позы покоя (стоим прямо).\n"
            "// Ограничиваем его по 0.35 м, чтобы игрок не улетал от тела при\n"
            "// калибровке (или когда HMD-мир смещён относительно центра комнаты).\n"
            "//-----------------------------------------------------------------------------\n"
            "namespace vr_camera\n"
            "{\n"
            "static vrmath::Pose s_restPose{};\n"
            "static bool        s_restValid = false;\n"
            "static vrmath::Vec3 s_smooth{};\n"
            "\n"
            "inline vrmath::Vec3 ClampOffset(const vrmath::Vec3& v, float maxLen)\n"
            "{\n"
            "    const float len = v.Length();\n"
            "    if (len > maxLen && len > 1e-5f)\n"
            "        return v * (maxLen / len);\n"
            "    return v;\n"
            "}\n"
            "\n"
            "vrmath::Vec3 GetHeadLocalOffset()\n"
            "{\n"
            "    vrmath::Pose head;\n"
            "    if (!vr::GetHeadPose(head))\n"
            "        return {};\n"
            "\n"
            "    if (!s_restValid)\n"
            "    {\n"
            "        s_restPose = head;\n"
            "        s_restValid = true;\n"
            "        return {};\n"
            "    }\n"
            "\n"
            "    if (vr::ConsumeRecenter())\n"
            "    {\n"
            "        s_restPose = head;\n"
            "        s_smooth = {};\n"
            "        return {};\n"
            "    }\n"
            "\n"
            "    vrmath::Vec3 d = head.p - s_restPose.p;\n"
            "    // Убираем вертикаль: за высоту камеры отвечает игра (рост/присед)\n"
            "    d.y = 0.f;\n"
            "    d = ClampOffset(d, 0.35f);\n"
            "\n"
            "    // Сглаживание (10% за кадр)\n"
            "    s_smooth = s_smooth + (d - s_smooth) * 0.10f;\n"
            "    return s_smooth;\n"
            "}\n"
            "} // namespace vr_camera\n"
        )
        s = s.replace("void CCameraFirstEye::Update(Fvector& point, Fvector& noise_dangle)",
                      helper + "\nvoid CCameraFirstEye::Update(Fvector& point, Fvector& noise_dangle)", 1)

    # Подключаем смещение в vPosition
    if "vr_camera::GetHeadLocalOffset();" not in s:
        s = s.replace("    vPosition.set(point);\n",
                      "    vPosition.set(point);\n\n"
                      "    // --- VR: реальное смещение головы (шаг/HMD) ---\n"
                      "    if (vr::IsActive())\n"
                      "    {\n"
                      "        const vrmath::Vec3 d = vr_camera::GetHeadLocalOffset();\n"
                      "        vPosition.x += d.x;\n"
                      "        vPosition.z += d.z;\n"
                      "    }\n", 1)

    write(f, s)
    print(f"[02] patched {f}")

if __name__ == "__main__":
    patch_cpp(ROOT)
