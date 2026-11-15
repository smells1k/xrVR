//-----------------------------------------------------------------------------
// xrVR.h - публичный API VR-модуля для движка X-Ray (OGSR Engine).
//
// Компилируется в трёх проектах:
//   XR_3DA     - Device/камера/консоль/UI-логика
//   xrGame     - поза головы -> управление актором, стрельба
//   xrRender_R4- стерео-рендер и SteamVR
//
// Здесь НЕТ типов из D3D11/OpenVR - только игровые типы и POD.
//-----------------------------------------------------------------------------
#pragma once

#include "xrVR_math.h"

// Fvector - это typedef в xrCore (typedef _vector3<float> Fvector;),
// предобъявлять его через "struct" нельзя: MSVC выдаст
// "using typedef-name 'Fvector' after 'struct'".
// _vector3d.h защищён #pragma once, повторное включение безопасно.
// Путь "../xrCore/_vector3d.h" работает, т.к. модуль всегда лежит в
// ogsr_engine/xrVR/ (см. integrate_vcxproj.py).
#include "../xrCore/_vector3d.h"

class IConsole_Command;

namespace vr
{
using Pose = vrmath::Pose;

//-------------------------------------------------------------------------
// Конфиг (gamedata/configs/vr/vr_config.ltx), секция [vr]
//-------------------------------------------------------------------------
struct Config
{
    bool  enabled{true};            // vr_enabled
    bool  autoEnableInGame{true};   // vr_auto_in_game

    float worldScale{1.0f};         // vr_world_scale: 1 игровой метр = N реальных
    float eyeHeight{1.78f};         // vr_eye_height: рост игрока (м)
    float ipdScale{1.0f};           // vr_ipd_scale

    float fovDeg{100.f};            // vr_fov: вертикальный FOV рендера
    float nearPlane{0.08f};         // vr_near
    float farPlane{1200.f};         // vr_far

    float hudDistance{0.75f};       // vr_hud_distance: дистанция VR-экрана (м)
    float hudSize{0.85f};           // vr_hud_size: высота VR-экрана (м)

    int   mirrorMode{1};            // vr_mirror: 0 - выкл, 1 - левый глаз, 2 - правый
    int   msaa{1};                  // vr_msaa
    float maxFPS{0.f};              // vr_max_fps: 0 = авто (частота HMD)

    bool  weaponAim{true};          // vr_weapon_aim: стрелять по взгляду из ствола
    bool  hmdfreeLook{true};        // vr_head_look: голова = отдельно от тела
    bool  hudInVR{true};            // vr_hud: показывать HUD/UI в шлеме
    float posePredictMs{0.f};       // vr_predict_ms
};

struct State
{
    bool  initialized{};    // OpenVR поднят
    bool  active{};         // стерео-режим активен (идёт игра)
    bool  inGame{};         // загружен уровень
    int   eye{};            // текущий рендерящийся глаз
    float fps{};            // сглаженный FPS
    Pose  head{};           // поза головы (X-Ray-конвенция)
    Pose  eyeToHead[2]{};   // смещение глаз относительно головы
    char  hmdName[128]{};
};

//-------------------------------------------------------------------------
// Состояние/конфиг
//-------------------------------------------------------------------------
State& GetState();
Config& GetConfig();

bool IsInitialized();
bool IsActive();     // можно ли сейчас рендерить стерео

//-------------------------------------------------------------------------
// Жизненный цикл (вызывается из движка)
//-------------------------------------------------------------------------
bool Init();
void Shutdown();
void LoadConfig();

void OnLevelLoad();
void OnLevelUnload();
void OnDeviceCreate();   // D3D11 создан (xrEngine)
void OnDeviceDestroy();

// Кадр: снимает позы, считает FPS/масштаб
void OnFrameBegin();
void OnFrameEnd();

//-------------------------------------------------------------------------
// Камера / взгляд
//-------------------------------------------------------------------------
// Поза головы в "мировых" координатах игрока (без покачиваний и т.п.)
bool GetHeadPose(Pose& out_pose);

// Текущее "желаемое" направление взгляда HMD (yaw/pitch в радианах).
// Возвращает false, если VR не активен (тогда камера - обычная мышь).
bool GetHeadLook(float& out_yaw, float& out_pitch, float& out_roll);

// Сброс/центрирование (vr_recenter)
void Recenter();
bool ConsumeRecenter();

//-------------------------------------------------------------------------
// Оружие: скорректировать точку/направление выстрела по взгляду HMD.
// Вызывается из CActor::g_fireParams / CWeapon::Fire
//-------------------------------------------------------------------------
void AdjustFireParams(Fvector& fire_pos, Fvector& fire_dir);

//-------------------------------------------------------------------------
// Консоль/логирование
//-------------------------------------------------------------------------
void RegisterConsoleCommands();
const char* GetStatusString();
void LogLine(const char* fmt, ...);
extern bool g_consoleRegistered;

} // namespace vr
