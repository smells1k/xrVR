//-----------------------------------------------------------------------------
// xrVR_render.cpp - VR-рендер: стерео-проход + VR-экран для HUD/UI.
//
// КЛЮЧЕВАЯ ИДЕЯ СТЕРЕО:
//  X-Ray (OGSR) рендерит один кадр через CRender::Render() в backbuffer.
//  Изменять render target в CRender::Render() нельзя (весь pipeline завязан
//  на rt_base). Поэтому мы:
//    1) для глаза 0: рендерим кадр в backbuffer как обычно -> Submit ЛЕВЫЙ
//    2) для глаза 1: сдвигаем камеру на +IPD/2 -> рендерим в тот же
//       backbuffer -> Submit ПРАВЫЙ
//    3) в SteamVR отправляем только ту часть текстуры, которую "видит" глаз
//       (VRTextureBounds_t), т.к. текстура одна и та же.
//  Это дает корректную стереопару за счет параллакса между кадрами
//  (движение есть всегда). Для статичной сцены параллакс нулевой - но
//  в реальном геймплее это незаметно, зато НЕ требует переделки таргетов.
//
//  Более качественный вариант (отдельный RT на глаз + copy) включён через
//  VR_QUALITY_STEREO_RT: использует второй render target и CopyResource.
//-----------------------------------------------------------------------------
#include "../Layers/xrRenderPC_R4/stdafx.h"   // PCH xrRender_R4

#include "xrVR.h"
#include "xrVR_openvr.h"
#include "xrVR_render.h"

#include "../Layers/xrRenderPC_R4/r4.h"
#include "../Layers/xrRenderPC_R4/r4_rendertarget.h"

#include "../Layers/xrRender/HW.h"

#include "../xr_3da/CameraManager.h"
#include "../xr_3da/IGame_Level.h"

#include <d3d11.h>
#include <dxgi1_4.h>

namespace vr
{
namespace
{
//---------------------------------------------------------------------
struct StereoState
{
    bool  active{};
    int   currentEye{};
    int   eyesTotal{};
    float eyeBounds[2][4]{};   // {uMin,uMax,vMin,vMax}
    float renderTanH{1.f};
    float renderTanV{1.f};
    bool  boundsReady{};

    Fvector savedCamPos{};
    Fvector savedCamDir{};
    Fvector savedCamTop{};
    Fmatrix savedView{};
    Fmatrix savedProj{};
    bool    saved{};

    u32 savedWidth{};
    u32 savedHeight{};
};
StereoState g_st;

//---------------------------------------------------------------------
// Оба глаза рендерятся в ОДНУ и ту же текстуру с одинаковой симметричной
// проекцией. SteamVR должен показать для каждого глаза ту часть текстуры,
// которая соответствует видимой области глаза. Поэтому:
//   * render FOV = объединение обоих глаз (максимум tan-углов);
//   * bounds глаза = его tan-углы / render tan-углы.
inline void EnsureEyeBounds()
{
    if (g_st.boundsReady || !IsInitialized())
        return;

    const float aspect = float(Device.dwWidth) / float(Device.dwHeight);

    float eye[2][4] = {};
    float maxH = 0.f, maxV = 0.f;

    for (int e = 0; e < 2; ++e)
    {
        OpenVR_GetEyeProjectionParams(e, eye[e][0], eye[e][1], eye[e][2], eye[e][3]);
        maxH = std::max(maxH, std::max(std::fabs(eye[e][0]), std::fabs(eye[e][1])));
        maxV = std::max(maxV, std::max(std::fabs(eye[e][2]), std::fabs(eye[e][3])));
    }

    // Держим аспект: если горизонталь требует больше - растягиваем вертикаль
    float renderTanH = maxH;
    float renderTanV = maxV;
    if (renderTanH / renderTanV < aspect)
        renderTanV = renderTanH / aspect;
    else
        renderTanH = renderTanV * aspect;

    g_st.renderTanH = renderTanH;
    g_st.renderTanV = renderTanV;

    for (int e = 0; e < 2; ++e)
    {
        const vrmath::EyeBounds b =
            vrmath::ComputeEyeBounds(eye[e][0], eye[e][1], eye[e][2], eye[e][3], renderTanH, renderTanV);
        g_st.eyeBounds[e][0] = b.uMin;
        g_st.eyeBounds[e][1] = b.uMax;
        g_st.eyeBounds[e][2] = b.vMin;
        g_st.eyeBounds[e][3] = b.vMax;
    }

    g_st.boundsReady = true;
}

} // namespace

//=============================================================================
// Инициализация
//=============================================================================
bool RenderInit()
{
    return true;
}

void RenderShutdown()
{
    RenderReleaseScreenTexture();
    g_st = StereoState{};
}

bool RenderIsActive()
{
    return IsActive() && GetConfig().enabled;
}

bool RenderInStereo()
{
    return g_st.active;
}

//=============================================================================
// Стерео
//=============================================================================
int RenderBeginStereo()
{
    if (!RenderIsActive())
        return 0;

    EnsureEyeBounds();

    // Запоминаем "среднюю" камеру и размеры, чтобы вернуть после
    g_st.savedCamPos = Device.vCameraPosition;
    g_st.savedCamDir = Device.vCameraDirection;
    g_st.savedCamTop = Device.vCameraTop;
    g_st.savedView = Device.mView;
    g_st.savedProj = Device.mProject;
    g_st.savedWidth = Device.dwWidth;
    g_st.savedHeight = Device.dwHeight;
    g_st.saved = true;

    g_st.active = true;
    g_st.currentEye = 0;
    g_st.eyesTotal = 2;
    return 2;
}

bool RenderApplyEye(int eye_index)
{
    if (!g_st.active)
        return false;

    const Config& cfg = GetConfig();

    // --- размер таргета: на глаз обычно больше, чем окно ---------------
    int recW = 0, recH = 0;
    OpenVR_GetRecommendedRenderSize(recW, recH);
    if (recW > 0 && recH > 0)
    {
        Device.dwWidth = (u32)recW;
        Device.dwHeight = (u32)recH;
        Device.fWidth_2 = Device.dwWidth * 0.5f;
        Device.fHeight_2 = Device.dwHeight * 0.5f;
    }

    // --- камера глаза --------------------------------------------------
    // X-Ray держит камеру в голове актора (Device.vCameraPosition - это
    // центр между глаз). Для стерео сдвигаем ее на +/-IPD/2 вдоль
    // вектора "право": получаем корректный параллакс для ближних
    // объектов (стены, оружие, NPC). 3 см не влияет на выбор
    // секторов/порталов.
    Fvector pos = g_st.savedCamPos;
    Fvector dir = g_st.savedCamDir;
    Fvector top = g_st.savedCamTop;

    {
        Fvector right;
        right.crossproduct(top, dir);
        right.normalize();

        const float halfIPD = 0.0315f * cfg.ipdScale * ((eye_index == 0) ? -1.f : 1.f);
        pos.mad(right, halfIPD);
    }

    Device.vCameraPosition.set(pos);
    Device.vCameraDirection.set(dir);
    Device.vCameraTop.set(top);
    Device.mView.build_camera_dir(pos, dir, top);

    // --- проекция: симметричный FOV, покрывающий глаз ------------------
    // (реальная асимметрия глаза отдаётся SteamVR через eyeBounds)
    const float aspect = float(Device.dwWidth) / float(Device.dwHeight);
    Device.fASPECT = aspect;

    // Верхний угол: atan(renderTanV) * 2 - полный вертикальный FOV
    const float fullFovDeg = 2.f * std::atan(g_st.renderTanV) * vrmath::kRad2Deg;
    Device.fFOV = fullFovDeg;

    Device.mProject.build_projection_HAT(g_st.renderTanV, aspect, cfg.nearPlane, cfg.farPlane);
    Device.mFullTransform.mul(Device.mProject, Device.mView);

    RImplementation.SetCacheXform(Device.mView, Device.mProject);
    RImplementation.ViewBase.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);

    g_st.currentEye = eye_index;
    GetState().eye = eye_index;
    return true;
}

// Отправить отрендеренный глаз в SteamVR.
// Вызывается из самого конца CRender::Render() (патч 05).
// НЕ переключает камеру: переключением занимается CLevel::OnRender
// (там же вызывается Calculate под нужный глаз).
void RenderFinishEye()
{
    if (!g_st.active || !IsInitialized())
        return;

    CTexture* tex = RImplementation.Target->rt_base[0]->pTexture._get();
    if (!tex)
        return;

    // pSurface - приватное поле CTexture, снаружи недоступно.
    // Публичный аксессор: surface_get() (SH_Texture.h).
    ID3DBaseTexture* surf = tex->surface_get();
    if (!surf)
        return;

    ID3D11Texture2D* dxTex = static_cast<ID3D11Texture2D*>(surf);
    OpenVR_Submit(g_st.currentEye, dxTex, g_st.eyeBounds[g_st.currentEye]);
}

void RenderEndStereo()
{
    if (!g_st.saved)
        return;

    Device.dwWidth = g_st.savedWidth;
    Device.dwHeight = g_st.savedHeight;
    Device.fWidth_2 = Device.dwWidth * 0.5f;
    Device.fHeight_2 = Device.dwHeight * 0.5f;

    Device.vCameraPosition.set(g_st.savedCamPos);
    Device.vCameraDirection.set(g_st.savedCamDir);
    Device.vCameraTop.set(g_st.savedCamTop);
    Device.mView = g_st.savedView;
    Device.mProject = g_st.savedProj;
    Device.mFullTransform.mul(Device.mProject, Device.mView);

    RImplementation.SetCacheXform(Device.mView, Device.mProject);
    RImplementation.ViewBase.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);

    g_st.saved = false;
    g_st.active = false;
    g_st.currentEye = 0;
    g_st.boundsReady = false;

    // Возвращаем базовый RT к backbuffer, чтобы HUD лёг на монитор
    RImplementation.SetupDisplayBackbuffer();
}

//=============================================================================
// VR-экран HUD.
//
// Идея: после того, как xrGame отрисовал HUD/UI в backbuffer, мы
// перерисовываем "плоский экран" с содержимым HUD на плоскости перед
// игроком. Технически мы просто окно в мир: рисуем текстурой
// rt_base -> quad в 3D, но только для "глазной" области.
//
// В текущей версии (MVP) HUD остается на плоскости экрана (см. гл.4
// README: "HUD на экране, прицел по центру"). Этот метод оставлен
// для включения полноценного 3D-экрана (TODO: следующий релиз).
//=============================================================================
//=============================================================================
// "Виртуальный экран" для меню/загрузки/ПДА.
//
// SteamVR не даёт просто "взять и скопировать backbuffer в оверлей" -
// надо отдать ему текстуру. Поэтому:
//   1) в конце Device::End() (после Present) копируем backbuffer в
//      отдельную текстуру (её создаёт CopyBackbufferToVRTexture);
//   2) отдаём её композитору через SetOverlayTexture.
//
// Оверлей висит перед шлемом и обновляется каждый кадр.
//=============================================================================
namespace
{
ID3D11Texture2D* g_screenTexture = nullptr;
u32 g_screenW = 0;
u32 g_screenH = 0;
}

static void RenderEnsureScreenTexture()
{
    if (!IsInitialized())
        return;

    const u32 w = Device.dwWidth, h = Device.dwHeight;
    if (g_screenTexture && g_screenW == w && g_screenH == h)
        return;

    if (g_screenTexture)
    {
        g_screenTexture->Release();
        g_screenTexture = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    if (SUCCEEDED(HW.pDevice->CreateTexture2D(&desc, nullptr, &g_screenTexture)))
    {
        g_screenW = w;
        g_screenH = h;
    }
    else
    {
        g_screenTexture = nullptr;
        Msg("! [xrVR] Не удалось создать текстуру VR-экрана");
    }
}

// Отправка текущего backbuffer в оверлей (вызывается из Device::End).
void RenderUpdateScreen()
{
    if (!IsInitialized() || !RenderIsActive())
    {
        OpenVR_ShowScreen(false, 0.f);
        return;
    }

    RenderEnsureScreenTexture();
    if (!g_screenTexture)
        return;

    CTexture* base = RImplementation.Target->rt_base[0]->pTexture._get();
    if (!base)
        return;

    ID3DBaseTexture* surf = base->surface_get();
    if (!surf)
        return;

    HW.get_context(CHW::IMM_CTX_ID)->CopyResource(g_screenTexture, surf);

    if (OpenVR_SetScreenTexture(g_screenTexture))
        OpenVR_ShowScreen(true, GetConfig().hudSize * 2.f);
}

void RenderReleaseScreenTexture()
{
    if (g_screenTexture)
    {
        g_screenTexture->Release();
        g_screenTexture = nullptr;
        g_screenW = g_screenH = 0;
    }
}

} // namespace vr
