//-----------------------------------------------------------------------------
// xrVR_openvr_null.cpp - "заглушка" OpenVR для проектов, где нет D3D11
// (XR_3DA и xrGame). Реальная реализация - xrVR_openvr.cpp в xrRender_R4,
// она вызывается через экспортный интерфейс рендера.
//
// Это позволяет XR_3DA/xrGame не зависеть от openvr_api.dll вообще.
//-----------------------------------------------------------------------------

#include "stdafx.h"   // PCH проекта-хозяина (имя ДОЛЖНО совпадать с /Yu)

#include "xrVR_openvr.h"

namespace vr
{
namespace
{
// Реализация из xrRender_R4. Если рендер без VR - указатели пустые,
// и все вызовы вернут "нет VR".
OpenVRImpl g_impl;
} // namespace

const OpenVRImpl* xrVR_GetOpenVRImpl() { return &g_impl; }

// Заполнение из xrRender_R4 (вызывается один раз при создании рендера)
void xrVR_BindOpenVRImpl(const OpenVRImpl* impl)
{
    if (impl)
        g_impl = *impl;
    else
        g_impl = OpenVRImpl{};
}

bool OpenVR_Init() { return g_impl.init ? g_impl.init() : false; }
void OpenVR_Shutdown() { if (g_impl.shutdown) g_impl.shutdown(); }
const char* OpenVR_GetHMDName() { return g_impl.hmd_name ? g_impl.hmd_name() : "n/a"; }
const char* OpenVR_GetRuntimeName() { return g_impl.runtime_name ? g_impl.runtime_name() : "n/a"; }
void OpenVR_BeginFrame() { if (g_impl.begin_frame) g_impl.begin_frame(); }
void OpenVR_EndFrame() { if (g_impl.end_frame) g_impl.end_frame(); }

void OpenVR_GetRawHeadPose(Pose& out) { out = Pose{}; if (g_impl.get_head) g_impl.get_head(&out); }
void OpenVR_GetGameHeadPose(Pose& out) { out = Pose{}; if (g_impl.get_game_head) g_impl.get_game_head(&out); }
void OpenVR_GetEyeToHead(int i, Pose& out) { out = Pose{}; if (g_impl.get_eye_to_head) g_impl.get_eye_to_head(i, &out); }

void OpenVR_GetEyeProjectionParams(int eye, float& l, float& r, float& t, float& b)
{
    l = -1.f; r = 1.f; t = 1.f; b = -1.f;
    if (g_impl.get_eye_proj) g_impl.get_eye_proj(eye, l, r, t, b);
}

void OpenVR_GetRecommendedRenderSize(int& w, int& h)
{
    w = 1920; h = 1080;
    if (g_impl.get_render_size) g_impl.get_render_size(w, h);
}

void OpenVR_GetHMDRefreshRate(float& hz)
{
    hz = 90.f;
    if (g_impl.get_refresh) g_impl.get_refresh(hz);
}

bool OpenVR_Submit(int eye, void* tex, const float uv[4])
{
    return g_impl.submit ? g_impl.submit(eye, tex, uv) : false;
}

void OpenVR_ShowScreen(bool show, float width_meters)
{
    if (g_impl.show_screen)
        g_impl.show_screen(show, width_meters);
}

bool OpenVR_SetScreenTexture(void* tex)
{
    return g_impl.set_screen_texture ? g_impl.set_screen_texture(tex) : false;
}

} // namespace vr
