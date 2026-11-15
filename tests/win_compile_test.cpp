// Проверка компиляции OpenVR-обёртки под Windows-компилятором (mingw).
// Проверяет только синтаксис/API openvr.h, без линковки.
#include "src/xrVR/xrVR_math.h"
#include <windows.h>
#include <d3d11.h>

#pragma warning(push)
#include "third_party/openvr.h"
#pragma warning(pop)

int main()
{
    vr::EVRInitError err = vr::VRInitError_None;
    (void)err;

    // Проверяем, что нужные символы существуют
    vr::IVRSystem* sys = nullptr;
    vr::IVRCompositor* comp = nullptr;
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::HmdMatrix34_t eye = {};
    vr::HmdMatrix44_t proj = {};
    vr::Texture_t tex = {};
    vr::VRTextureBounds_t bounds = {};

    (void)sys; (void)comp; (void)poses; (void)eye; (void)proj; (void)tex; (void)bounds;

    // Конверсия позы из OpenVR в X-Ray
    float m[12] = {1,0,0,0, 0,1,0,1.6f, 0,0,1,0};
    vrmath::Pose p = vrmath::PoseFromOpenVR(m, 1.f);
    (void)p;

    return 0;
}
