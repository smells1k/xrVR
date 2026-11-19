//-----------------------------------------------------------------------------
// xrVR_openvr.cpp - РЕАЛЬНАЯ реализация OpenVR (проект xrRender_R4).
//
// ВАЖНО: этот файл НЕ определяет функции OpenVR_* (они определены в
// xrVR_openvr_null.cpp внутри XR_3DA). Вместо этого он экспортирует
// таблицу указателей vr::OvrGetImpl(), которую рендер привязывает к
// модулю при создании (см. патч 06-openvr-bind).
//
// Так XR_3DA и xrGame не линкуют openvr_api.dll вообще, а вся работа
// с OpenVR идёт из одного места - рендера.
//-----------------------------------------------------------------------------
#if defined(_WIN32)

#include "stdafx.h"   // PCH xrRender_R4 (имя ДОЛЖНО совпадать с /Yu)

#include "xrVR.h"
#include "xrVR_openvr.h"
#include "xrVR_math.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdarg>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "openvr.h"

namespace vr
{
namespace
{
::vr::IVRSystem* g_system = nullptr;
::vr::IVRCompositor* g_compositor = nullptr;
::vr::IVROverlay* g_overlay = nullptr;

char g_hmdName[128] = "unknown";
char g_runtimeName[128] = "unknown";

::vr::TrackedDevicePose_t g_renderPoses[::vr::k_unMaxTrackedDeviceCount];
::vr::TrackedDevicePose_t g_gamePoses[::vr::k_unMaxTrackedDeviceCount];
::vr::HmdMatrix34_t g_eyeToHead[2];

// ---- оверлей "виртуальный экран" ------------------------------------------
::vr::VROverlayHandle_t g_screenOverlay = ::vr::k_ulOverlayHandleInvalid;
bool g_screenOverlayVisible = false;

//---------------------------------------------------------------------------
void AppendLog(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

    typedef void(__cdecl* PFN_Log)(const char*);
    static PFN_Log pfn = nullptr;
    static bool resolved = false;
    if (!resolved)
    {
        resolved = true;
        HMODULE exe = GetModuleHandleA(nullptr);
        if (exe)
            pfn = reinterpret_cast<PFN_Log>(GetProcAddress(exe, "?Log@@YAXPBD@Z"));
    }
    if (pfn)
        pfn(buf);
}

inline void Mat34ToPose(const ::vr::HmdMatrix34_t& m, float scale, vrmath::Pose& out)
{
    const float raw[12] = {
        m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3],
        m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
        m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3],
    };
    out = vrmath::PoseFromOpenVR(raw, scale);
}

//===========================================================================
// Функции таблицы
//===========================================================================
bool ImplInit()
{
    if (g_system)
        return true;

    ::vr::EVRInitError err = ::vr::VRInitError_None;
    g_system = ::vr::VR_Init(&err, ::vr::VRApplication_Scene);
    if (!g_system || err != ::vr::VRInitError_None)
    {
        AppendLog("! [xrVR] VR_Init failed (%d): %s", (int)err, ::vr::VR_GetVRInitErrorAsEnglishDescription(err));
        g_system = nullptr;
        return false;
    }

    g_compositor = reinterpret_cast<::vr::IVRCompositor*>(::vr::VR_GetGenericInterface(::vr::IVRCompositor_Version, &err));
    if (!g_compositor || err != ::vr::VRInitError_None)
    {
        AppendLog("! [xrVR] IVRCompositor failed (%d)", (int)err);
        ::vr::VR_Shutdown();
        g_system = nullptr;
        g_compositor = nullptr;
        return false;
    }

    g_overlay = reinterpret_cast<::vr::IVROverlay*>(::vr::VR_GetGenericInterface(::vr::IVROverlay_Version, &err));
    if (err != ::vr::VRInitError_None)
        g_overlay = nullptr;

    ::vr::ETrackedPropertyError perr = ::vr::TrackedProp_Success;
    g_system->GetStringTrackedDeviceProperty(::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_ModelNumber_String, g_hmdName, sizeof(g_hmdName), &perr);
    g_system->GetStringTrackedDeviceProperty(::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_DriverVersion_String, g_runtimeName, sizeof(g_runtimeName), &perr);

    g_eyeToHead[0] = g_system->GetEyeToHeadTransform(::vr::Eye_Left);
    g_eyeToHead[1] = g_system->GetEyeToHeadTransform(::vr::Eye_Right);

    // Композитор должен работать в D3D11
    if (g_compositor->GetCurrentSceneFocusProcess() == 0)
    {
        // не критично - при первом Submit поднимется
    }

    AppendLog("* [xrVR] OpenVR online. HMD: %s, runtime: %s", g_hmdName, g_runtimeName);
    return true;
}

void ImplShutdown()
{
    if (g_overlay && g_screenOverlay != ::vr::k_ulOverlayHandleInvalid)
    {
        g_overlay->DestroyOverlay(g_screenOverlay);
        g_screenOverlay = ::vr::k_ulOverlayHandleInvalid;
    }

    g_overlay = nullptr;
    g_compositor = nullptr;
    g_system = nullptr;
    g_screenOverlayVisible = false;

    ::vr::VR_Shutdown();
    AppendLog("* [xrVR] OpenVR shutdown.");
}

const char* ImplHmdName() { return g_hmdName; }
const char* ImplRuntimeName() { return g_runtimeName; }

void ImplBeginFrame()
{
    if (!g_system || !g_compositor)
        return;

    // Ждём "окно" композитора - он сам задаёт ритм кадров (90/120 Гц)
    g_compositor->WaitGetPoses(g_renderPoses, ::vr::k_unMaxTrackedDeviceCount, nullptr, 0);
    memcpy(g_gamePoses, g_renderPoses, sizeof(g_renderPoses));
}

void ImplEndFrame()
{
    // Не нужен: Submit уже сделан в рендере
}

void ImplGetHead(Pose* out)
{
    if (!out)
        return;
    *out = Pose{};
    if (!g_system)
        return;

    const auto& p = g_renderPoses[::vr::k_unTrackedDeviceIndex_Hmd];
    if (!p.bPoseIsValid || !p.bDeviceIsConnected)
        return;

    Mat34ToPose(p.mDeviceToAbsoluteTracking, 1.f, *out);
    out->valid = true;
}

void ImplGetGameHead(Pose* out)
{
    if (!out)
        return;
    *out = Pose{};
    if (!g_system)
        return;

    const auto& p = g_gamePoses[::vr::k_unTrackedDeviceIndex_Hmd];
    if (!p.bPoseIsValid || !p.bDeviceIsConnected)
        return;

    Mat34ToPose(p.mDeviceToAbsoluteTracking, 1.f, *out);
    out->valid = true;
}

void ImplGetEyeToHead(int eye, Pose* out)
{
    if (!out)
        return;
    *out = Pose{};
    if (!g_system || eye < 0 || eye > 1)
        return;
    Mat34ToPose(g_eyeToHead[eye], 1.f, *out);
    out->valid = true;
}

void ImplGetEyeProj(int eye, float& l, float& r, float& t, float& b)
{
    l = -1.f; r = 1.f; t = 1.f; b = -1.f;
    if (!g_system)
        return;

    const ::vr::HmdMatrix44_t m =
        g_system->GetProjectionMatrix(eye == 0 ? ::vr::Eye_Left : ::vr::Eye_Right, 0.05f, 4096.f);

    const float m00 = m.m[0][0];
    const float m02 = m.m[2][0];
    const float m11 = m.m[1][1];
    const float m12 = m.m[2][1];
    if (std::fabs(m00) < 1e-6f || std::fabs(m11) < 1e-6f)
        return;

    r = (1.f + m02) / m00;
    l = -(1.f - m02) / m00;
    t = (1.f + m12) / m11;
    b = -(1.f - m12) / m11;
}

void ImplGetRenderSize(int& w, int& h)
{
    w = 1920; h = 1080;
    if (!g_system)
        return;
    uint32_t rw = 0, rh = 0;
    g_system->GetRecommendedRenderTargetSize(&rw, &rh);
    if (rw && rh) { w = (int)rw; h = (int)rh; }
}

void ImplGetRefresh(float& hz)
{
    hz = 90.f;
    if (!g_system)
        return;
    const float r = g_system->GetFloatTrackedDeviceProperty(::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_DisplayFrequency_Float);
    if (r > 30.f) hz = r;
}

bool ImplSubmit(int eye, void* texture, const float uv[4])
{
    if (!g_system || !g_compositor || !texture)
        return false;

    ::vr::Texture_t tex{};
    tex.handle = texture;
    tex.eType = ::vr::TextureType_DirectX;
    tex.eColorSpace = ::vr::ColorSpace_Auto;

    ::vr::VRTextureBounds_t bounds{};
    if (uv) { bounds.uMin = uv[0]; bounds.uMax = uv[1]; bounds.vMin = uv[2]; bounds.vMax = uv[3]; }
    else    { bounds.uMin = 0.f; bounds.uMax = 1.f; bounds.vMin = 0.f; bounds.vMax = 1.f; }

    const auto e = g_compositor->Submit(eye == 0 ? ::vr::Eye_Left : ::vr::Eye_Right, &tex, &bounds, ::vr::Submit_Default);
    if (e != ::vr::VRCompositorError_None)
    {
        static int logged = 0;
        if (logged < 5) { AppendLog("! [xrVR] Submit(%d) failed: %d", eye, (int)e); ++logged; }
        return false;
    }
    return true;
}

//===========================================================================
// Виртуальный экран (меню/ПДА/загрузка) как overlay SteamVR
//===========================================================================
bool EnsureScreenOverlay()
{
    if (!g_overlay)
        return false;
    if (g_screenOverlay != ::vr::k_ulOverlayHandleInvalid)
        return true;

    const ::vr::EVROverlayError e = g_overlay->CreateOverlay("stalker_vr_screen", "STALKER VR Screen", &g_screenOverlay);
    if (e != ::vr::VROverlayError_None)
    {
        AppendLog("! [xrVR] CreateOverlay failed: %d", (int)e);
        g_screenOverlay = ::vr::k_ulOverlayHandleInvalid;
        return false;
    }
    return true;
}

void ImplShowScreen(bool show, float width_meters)
{
    if (!EnsureScreenOverlay())
        return;

    if (!show)
    {
        if (g_screenOverlayVisible)
        {
            g_overlay->HideOverlay(g_screenOverlay);
            g_screenOverlayVisible = false;
        }
        return;
    }

    // Ставим экран прямо перед шлемом, чуть ниже уровня глаз
    ::vr::HmdMatrix34_t transform{};
    transform.m[0][0] = 1.f; transform.m[1][1] = 1.f; transform.m[2][2] = 1.f;
    transform.m[1][3] = -0.10f;  // чуть ниже глаз
    transform.m[2][3] = -1.60f;  // 1.6 м перед лицом (в OpenVR -Z вперёд)

    g_overlay->SetOverlayTransformTrackedDeviceRelative(g_screenOverlay, ::vr::k_unTrackedDeviceIndex_Hmd, &transform);
    g_overlay->SetOverlayWidthInMeters(g_screenOverlay, width_meters > 0.2f ? width_meters : 1.6f);
    g_overlay->SetOverlayInputMethod(g_screenOverlay, ::vr::VROverlayInputMethod_None);
    g_overlay->SetOverlayAlpha(g_screenOverlay, 1.f);

    if (!g_screenOverlayVisible)
    {
        g_overlay->ShowOverlay(g_screenOverlay);
        g_screenOverlayVisible = true;
    }
}

bool ImplSetScreenTexture(void* d3d_texture)
{
    if (!g_overlay || g_screenOverlay == ::vr::k_ulOverlayHandleInvalid || !d3d_texture)
        return false;

    ::vr::Texture_t tex{};
    tex.handle = d3d_texture;
    tex.eType = ::vr::TextureType_DirectX;
    tex.eColorSpace = ::vr::ColorSpace_Auto;

    const ::vr::EVROverlayError e = g_overlay->SetOverlayTexture(g_screenOverlay, &tex);
    if (e != ::vr::VROverlayError_None)
        return false;
    return true;
}

} // namespace

//===========================================================================
// Таблица реализации для движка
//===========================================================================
const OpenVRImpl* OvrGetImpl()
{
    static OpenVRImpl impl{};
    impl.init = &ImplInit;
    impl.shutdown = &ImplShutdown;
    impl.hmd_name = &ImplHmdName;
    impl.runtime_name = &ImplRuntimeName;
    impl.begin_frame = &ImplBeginFrame;
    impl.end_frame = &ImplEndFrame;
    impl.get_head = &ImplGetHead;
    impl.get_game_head = &ImplGetGameHead;
    impl.get_eye_to_head = &ImplGetEyeToHead;
    impl.get_eye_proj = &ImplGetEyeProj;
    impl.get_render_size = &ImplGetRenderSize;
    impl.get_refresh = &ImplGetRefresh;
    impl.submit = &ImplSubmit;
    impl.show_screen = &ImplShowScreen;
    impl.set_screen_texture = &ImplSetScreenTexture;
    return &impl;
}

} // namespace vr

#endif // _WIN32
