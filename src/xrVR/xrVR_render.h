//-----------------------------------------------------------------------------
// xrVR_render.h - интерфейс VR-рендера (xrRender_R4 <-> XR_3DA/xrGame).
//
// Паттерн вызовов (см. патчи 04/05/06):
//
//   [CLevel::OnRender]                   [CRender::Render]
//   if (vr::RenderBeginStereo() == 2)        ...
//   {                                       vr::RenderFinishEye();  // Submit глаза
//       for (eye = 0; eye < 2; ++eye)
//       {
//           vr::RenderApplyEye(eye);    // камера/проекция/размер
//           Render->Calculate();        // видимость под глаз
//           Render->Render();           // отрисовка + Submit
//       }
//       vr::RenderEndStereo();          // вернуть камеру/backbuffer
//   }
//-----------------------------------------------------------------------------
#pragma once

#include "xrVR_math.h"

namespace vr
{
bool RenderInit();
void RenderShutdown();

bool RenderIsActive();

// Начать стерео-проход. Возвращает число глаз (0 - VR выключен).
int  RenderBeginStereo();

// Применить камеру/проекцию глаза.
bool RenderApplyEye(int eye_index);

// Отправить текущий глаз в SteamVR; если есть второй - переключить камеру.
void RenderFinishEye();

// Вернуть исходную камеру/размеры/backbuffer (вызывается из CLevel::OnRender).
void RenderEndStereo();

// Прямой доступ: "VR включён и сейчас надо рендерить в стерео"
bool RenderInStereo();

// Виртуальный экран для меню/загрузки/ПДА (SteamVR overlay).
// RenderUpdateScreen() вызывается из CRenderDevice::End() (раз в кадр).
void RenderUpdateScreen();
void RenderReleaseScreenTexture();

} // namespace vr
