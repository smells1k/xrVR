//-----------------------------------------------------------------------------
// xrVR_openvr.h - интерфейс OpenVR-обёртки и таблица реализации.
//
// XR_3DA/xrGame работают через xrVR_openvr_null.cpp (нулевая реализация),
// xrRender_R4 предоставляет настоящую реализацию (xrVR_openvr_r4.cpp)
// через таблицу vr::OpenVRImpl. Связывание делает рендер при старте.
//-----------------------------------------------------------------------------
#pragma once

#include "xrVR_math.h"

namespace vr
{
using Pose = vrmath::Pose;

//---------------------------------------------------------------------------
// Таблица функций OpenVR. Заполняется рендером (xrRender_R4).
// Все функции - "сырые": работают с X-Ray-конвенцией (+Y up, +Z fwd).
//---------------------------------------------------------------------------
struct OpenVRImpl
{
    bool (*init)() = nullptr;
    void (*shutdown)() = nullptr;

    const char* (*hmd_name)() = nullptr;
    const char* (*runtime_name)() = nullptr;

    void (*begin_frame)() = nullptr;   // WaitGetPoses
    void (*end_frame)() = nullptr;

    void (*get_head)(Pose*) = nullptr;       // поза для рендера
    void (*get_game_head)(Pose*) = nullptr;  // поза для игры
    void (*get_eye_to_head)(int, Pose*) = nullptr;

    // tan-углы глаза (l,r,t,b). Знак: l<0, b<0
    void (*get_eye_proj)(int, float&, float&, float&, float&) = nullptr;

    void (*get_render_size)(int&, int&) = nullptr;  // рекомендованный размер на глаз
    void (*get_refresh)(float&) = nullptr;          // частота HMD

    // Отправка текстуры глаза в композитор; uv = {uMin,uMax,vMin,vMax}
    bool (*submit)(int eye, void* d3d11_texture, const float uv[4]) = nullptr;

    // Виртуальный экран (меню/ПДА) в виде SteamVR-overlay
    void (*show_screen)(bool, float width_meters) = nullptr;
    bool (*set_screen_texture)(void* d3d11_texture) = nullptr;
};

// Реализация (возвращается рендером, привязывается движком)
const OpenVRImpl* OvrGetImpl();

// Привязка/использование (реализуется в xrVR_openvr_null.cpp)
void xrVR_BindOpenVRImpl(const OpenVRImpl* impl);
const OpenVRImpl* xrVR_GetOpenVRImpl();

//---------------------------------------------------------------------------
// Универсальные обёртки (используются движком; внутри - таблица выше).
//---------------------------------------------------------------------------
bool OpenVR_Init();
void OpenVR_Shutdown();

const char* OpenVR_GetHMDName();
const char* OpenVR_GetRuntimeName();

void OpenVR_BeginFrame();
void OpenVR_EndFrame();

void OpenVR_GetRawHeadPose(Pose& out);
void OpenVR_GetGameHeadPose(Pose& out);
void OpenVR_GetEyeToHead(int eye_index, Pose& out);

void OpenVR_GetEyeProjectionParams(int eye_index, float& tanL, float& tanR, float& tanT, float& tanB);
void OpenVR_GetRecommendedRenderSize(int& w, int& h);
void OpenVR_GetHMDRefreshRate(float& hz);

bool OpenVR_Submit(int eye_index, void* d3d11_texture, const float uv[4]);

void OpenVR_ShowScreen(bool show, float width_meters);
bool OpenVR_SetScreenTexture(void* d3d11_texture);

} // namespace vr
