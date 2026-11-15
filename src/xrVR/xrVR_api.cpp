//-----------------------------------------------------------------------------
// xrVR_api.cpp - реализация VR-API (вызовы уже из ОБЫЧНЫХ проектов OGSR,
// никаких трейси/профайлеров здесь нет).
//
// Компилируется в XR_3DA и xrGame (и линкуется как xrEngine.lib).
// D3D11/OpenVR - за интерфейсом xrVR_openvr.h, который в этих проектах
// реализуется как null-stub.
// Created for S.T.A.L.K.E.R. Shadow of Chernobyl (OGSR Engine).
//-----------------------------------------------------------------------------
#include "../xr_3da/stdafx.h"   // PCH XR_3DA (xrGame подключает его из своего stdafx.h)

#include "xrVR.h"
#include "xrVR_openvr.h"
#include "xrVR_console.h"

#include "../xrCore/xr_ini.h"
#include "../xrCore/log.h"

#include <cmath>
#include <cstdarg>
#include <cstring>

namespace vr
{
bool g_consoleRegistered = false;

namespace
{
State g_state;
Config g_config;

bool g_recenterPending = false;

// Поза прошлого кадра (для дельт)
Pose g_prevHead;
bool g_prevHeadValid = false;
bool g_haveHead = false;
Pose g_head;

// Внутренний FPS-счётчик
struct FpsCounter
{
    u64 lastTick = 0;
    float accum = 0.f;
    int frames = 0;
};
FpsCounter g_fps;

inline float RadToDeg(float r) { return r * vrmath::kRad2Deg; }

} // namespace

//=============================================================================
// Состояние/конфиг
//=============================================================================
State& GetState() { return g_state; }
Config& GetConfig() { return g_config; }

bool IsInitialized() { return g_state.initialized; }
bool IsActive() { return g_state.initialized && g_state.active && g_state.inGame; }

//=============================================================================
// Логирование (общая точка для всех проектов)
//=============================================================================
void LogLine(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Log(buf);
}

//=============================================================================
// Инициализация
//=============================================================================
bool Init()
{
    if (g_state.initialized)
        return true;

    if (!OpenVR_Init())
    {
        LogLine("! [xrVR] Не удалось запустить OpenVR (SteamVR не установлен/не запущен).");
        return false;
    }

    g_state.initialized = true;
    strncpy_s(g_state.hmdName, OpenVR_GetHMDName(), _TRUNCATE);

    // Сообщаем композитору, что приложение - контент для сцены
    OpenVR_GetEyeToHead(0, g_state.eyeToHead[0]);
    OpenVR_GetEyeToHead(1, g_state.eyeToHead[1]);

    float hz = 0.f;
    OpenVR_GetHMDRefreshRate(hz);

    LogLine("* [xrVR] VR запущен. Шлем: %s, частота: %.0f Hz", g_state.hmdName, hz);
    return true;
}

void Shutdown()
{
    if (!g_state.initialized)
        return;

    OpenVR_Shutdown();

    g_state = State{};
    g_haveHead = false;
    g_prevHeadValid = false;

    LogLine("* [xrVR] VR выключен.");
}

//=============================================================================
// Конфиг
//=============================================================================
void LoadConfig()
{
    Config& c = g_config;
    if (!pSettings || !pSettings->section_exist("vr"))
    {
        LogLine("* [xrVR] секции [vr] нет в configs/vr/vr_config.ltx - настройки по умолчанию.");
        return;
    }

    auto get_bool = [](LPCSTR n, bool def) {
        return pSettings->line_exist("vr", n) ? (CInifile::IsBOOL(pSettings->r_string("vr", n)) != FALSE) : def;
    };
    auto get_float = [](LPCSTR n, float def) { return pSettings->line_exist("vr", n) ? pSettings->r_float("vr", n) : def; };
    auto get_int = [](LPCSTR n, int def) { return pSettings->line_exist("vr", n) ? (int)pSettings->r_u32("vr", n) : def; };

    c.enabled = get_bool("vr_enabled", c.enabled);
    c.autoEnableInGame = get_bool("vr_auto_in_game", c.autoEnableInGame);
    c.worldScale = get_float("vr_world_scale", c.worldScale);
    c.eyeHeight = get_float("vr_eye_height", c.eyeHeight);
    c.ipdScale = get_float("vr_ipd_scale", c.ipdScale);
    c.fovDeg = get_float("vr_fov", c.fovDeg);
    c.nearPlane = get_float("vr_near", c.nearPlane);
    c.farPlane = get_float("vr_far", c.farPlane);
    c.hudDistance = get_float("vr_hud_distance", c.hudDistance);
    c.hudSize = get_float("vr_hud_size", c.hudSize);
    c.mirrorMode = get_int("vr_mirror", c.mirrorMode);
    c.msaa = get_int("vr_msaa", c.msaa);
    c.maxFPS = get_float("vr_max_fps", c.maxFPS);
    c.weaponAim = get_bool("vr_weapon_aim", c.weaponAim);
    c.hmdfreeLook = get_bool("vr_head_look", c.hmdfreeLook);
    c.hudInVR = get_bool("vr_hud", c.hudInVR);
    c.posePredictMs = get_float("vr_predict_ms", c.posePredictMs);

    auto clampf = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    c.worldScale = clampf(c.worldScale, 0.3f, 3.f);
    c.eyeHeight = clampf(c.eyeHeight, 1.f, 2.2f);
    c.ipdScale = clampf(c.ipdScale, 0.5f, 2.f);
    c.fovDeg = clampf(c.fovDeg, 60.f, 130.f);
    c.nearPlane = clampf(c.nearPlane, 0.01f, 0.5f);
    c.farPlane = clampf(c.farPlane, 100.f, 5000.f);
    c.hudDistance = clampf(c.hudDistance, 0.25f, 3.f);
    c.hudSize = clampf(c.hudSize, 0.3f, 2.f);
    c.mirrorMode = c.mirrorMode < 0 ? 0 : (c.mirrorMode > 2 ? 2 : c.mirrorMode);

    LogLine("* [xrVR] config: enabled=%d scale=%.2f eye=%.2f fov=%.0f hudDist=%.2f hudSize=%.2f",
            c.enabled ? 1 : 0, c.worldScale, c.eyeHeight, c.fovDeg, c.hudDistance, c.hudSize);
}

void OnDeviceCreate()
{
    LoadConfig();
    RegisterConsoleCommands();
}

void OnDeviceDestroy()
{
    Shutdown();
}

//=============================================================================
// Уровень
//=============================================================================
void OnLevelLoad()
{
    g_state.inGame = true;
    Recenter();

    if (g_config.enabled && g_config.autoEnableInGame && !g_state.initialized)
        Init();
}

void OnLevelUnload()
{
    g_state.inGame = false;
    g_haveHead = false;
    g_prevHeadValid = false;
}

//=============================================================================
// Кадр
//=============================================================================
void OnFrameBegin()
{
    if (!g_state.initialized)
    {
        g_state.active = false;
        return;
    }

    OpenVR_BeginFrame();

    // Поза для игры
    Pose head;
    OpenVR_GetGameHeadPose(head);
    if (!head.valid)
    {
        OpenVR_GetRawHeadPose(head);
    }

    if (head.valid)
    {
        g_head = head;
        g_haveHead = true;
        g_state.head = head;
    }

    g_state.active = g_haveHead && g_state.inGame;

    // FPS-счётчик
    const u64 now = GetTickCount64();
    if (g_fps.lastTick)
    {
        const float dt = float(now - g_fps.lastTick) / 1000.f;
        if (dt > 0.f && dt < 1.f)
        {
            g_fps.accum += dt;
            ++g_fps.frames;
            if (g_fps.accum >= 0.5f)
            {
                g_state.fps = float(g_fps.frames) / g_fps.accum;
                g_fps.accum = 0.f;
                g_fps.frames = 0;
            }
        }
    }
    g_fps.lastTick = now;
}

void OnFrameEnd()
{
    if (g_state.initialized)
        OpenVR_EndFrame();
}

//=============================================================================
// Голова/взгляд
//=============================================================================
bool GetHeadPose(Pose& out_pose)
{
    if (!g_haveHead)
        return false;
    out_pose = g_head;
    return true;
}

bool GetHeadLook(float& out_yaw, float& out_pitch, float& out_roll)
{
    if (!IsActive() || !g_haveHead)
        return false;

    // Масштабируем позицию/углы под "рост" игрока: поза головы X-Ray-конвенции
    Pose head = vrmath::HeadToViewPose(g_head, g_config.eyeHeight);

    // Здесь важно: голова двигает КАМЕРУ (через CCameraFirstEye), а не тело.
    vrmath::ToYawPitchRollView(head.q, out_yaw, out_pitch, out_roll);

    // VR: "мир" зафиксирован, игрок стоит в нём. Но у X-Ray камера
    // привязана к точке актора, поэтому используем только углы, а
    // поступательное движение головы применяем к смещению камеры
    // (делается в CameraFirstEye.cpp).
    return true;
}

void Recenter()
{
    g_recenterPending = true;
    g_prevHeadValid = false;
}

bool ConsumeRecenter()
{
    if (g_recenterPending)
    {
        g_recenterPending = false;
        return true;
    }
    return false;
}

//=============================================================================
// Стрельба по взгляду HMD
//=============================================================================
void AdjustFireParams(Fvector& fire_pos, Fvector& fire_dir)
{
    if (!IsActive() || !g_config.weaponAim || !g_haveHead)
        return;

    const Pose head = vrmath::HeadToViewPose(g_head, g_config.eyeHeight);
    const vrmath::Vec3 dir = head.q.Rotate({0.f, 0.f, 1.f}).Normalized();

    fire_dir.set(dir.x, dir.y, dir.z);
    fire_dir.normalize();
}

//=============================================================================
// Статус
//=============================================================================
const char* GetStatusString()
{
    static char buf[1024];
    xr_sprintf(buf, sizeof(buf),
             "[xrVR] initialized=%s active=%s inGame=%s hmd=%s\n"
             "[xrVR] eyeHeight=%.2f fov=%.0f hudDist=%.2f hudSize=%.2f\n"
             "[xrVR] head valid=%s p=(%.2f %.2f %.2f) fps=%.0f",
             g_state.initialized ? "yes" : "no", g_state.active ? "yes" : "no", g_state.inGame ? "yes" : "no",
             g_state.initialized ? g_state.hmdName : "n/a", g_config.eyeHeight, g_config.fovDeg, g_config.hudDistance,
             g_config.hudSize, g_haveHead ? "yes" : "no", g_head.p.x, g_head.p.y, g_head.p.z, g_state.fps);
    return buf;
}

} // namespace vr
