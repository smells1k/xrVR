//-----------------------------------------------------------------------------
// xrVR_console.cpp - консольные команды VR-модуля (XR_3DA).
//
//   vr_on            - включить VR (поднять SteamVR-сессию)
//   vr_off           - disable VR
//   vr_status        - состояние
//   vr_recenter      - отцентровать позу
//   vr_scale <x>     - мировой масштаб (1.0 = метры)
//   vr_eye_height<x> - рост игрока
//   vr_hud_dist <x>  - дистанция VR-экрана HUD
//   vr_fov <deg>     - вертикальный FOV
//-----------------------------------------------------------------------------

#include "stdafx.h"   // PCH проекта-хозяина (имя ДОЛЖНО совпадать с /Yu)

#include "xrVR.h"
#include "xrVR_console.h"

#include "../xr_3da/XR_IOConsole.h"
#include "../xr_3da/xr_ioc_cmd.h"

namespace vr
{
namespace
{
//-------------------------------------------------------------------------
class CCC_VR_On : public IConsole_Command
{
public:
    CCC_VR_On() : IConsole_Command("vr_on") {}
    void Execute(LPCSTR args) override
    {
        if (args && (0 == xr_strcmp(args, "off") || 0 == xr_strcmp(args, "0")))
        {
            Shutdown();
            LogLine("* [xrVR] VR disabled by command.");
            return;
        }

        GetConfig().enabled = true;
        if (Init())
            LogLine("* [xrVR] VR enabled.");
        else
            LogLine("! [xrVR] Failed: SteamVR not running or HMD not connected.");
    }
    void Info(TInfo& I) override { xr_strcpy(I, "[on/off] - enable/disable VR"); }
};

class CCC_VR_Off : public IConsole_Command
{
public:
    CCC_VR_Off() : IConsole_Command("vr_off") {}
    void Execute(LPCSTR) override
    {
        Shutdown();
        LogLine("* [xrVR] VR disabled.");
    }
    void Info(TInfo& I) override { xr_strcpy(I, "- disable VR"); }
};

class CCC_VR_Status : public IConsole_Command
{
public:
    CCC_VR_Status() : IConsole_Command("vr_status") {}
    void Execute(LPCSTR) override { LogLine("%s", GetStatusString()); }
    void Info(TInfo& I) override { xr_strcpy(I, "- current VR state"); }
};

class CCC_VR_Recenter : public IConsole_Command
{
public:
    CCC_VR_Recenter() : IConsole_Command("vr_recenter") {}
    void Execute(LPCSTR) override
    {
        Recenter();
        LogLine("* [xrVR] Pose recentered.");
    }
    void Info(TInfo& I) override { xr_strcpy(I, "- recenter head/camera"); }
};

class CCC_VR_Scale : public IConsole_Command
{
public:
    CCC_VR_Scale() : IConsole_Command("vr_scale") {}
    void Execute(LPCSTR args) override
    {
        const float v = (float)atof(args);
        if (v < 0.3f || v > 3.f)
        {
            InvalidSyntax();
            return;
        }
        GetConfig().worldScale = v;
        LogLine("* [xrVR] world scale = %.2f", v);
    }
    void Info(TInfo& I) override { xr_strcpy(I, "<0.3..3.0> - world scale"); }
};

class CCC_VR_EyeHeight : public IConsole_Command
{
public:
    CCC_VR_EyeHeight() : IConsole_Command("vr_eye_height") {}
    void Execute(LPCSTR args) override
    {
        const float v = (float)atof(args);
        if (v < 1.f || v > 2.2f)
        {
            InvalidSyntax();
            return;
        }
        GetConfig().eyeHeight = v;
        LogLine("* [xrVR] eye height = %.2f m", v);
    }
    void Info(TInfo& I) override { xr_strcpy(I, "<1.0..2.2> - player height, m"); }
};

class CCC_VR_HudDist : public IConsole_Command
{
public:
    CCC_VR_HudDist() : IConsole_Command("vr_hud_dist") {}
    void Execute(LPCSTR args) override
    {
        const float v = (float)atof(args);
        if (v < 0.25f || v > 3.f)
        {
            InvalidSyntax();
            return;
        }
        GetConfig().hudDistance = v;
        LogLine("* [xrVR] hud distance = %.2f m", v);
    }
    void Info(TInfo& I) override { xr_strcpy(I, "<0.25..3.0> - HUD screen distance"); }
};

class CCC_VR_Fov : public IConsole_Command
{
public:
    CCC_VR_Fov() : IConsole_Command("vr_fov") {}
    void Execute(LPCSTR args) override
    {
        const float v = (float)atof(args);
        if (v < 60.f || v > 130.f)
        {
            InvalidSyntax();
            return;
        }
        GetConfig().fovDeg = v;
        LogLine("* [xrVR] render fov = %.0f", v);
    }
    void Info(TInfo& I) override { xr_strcpy(I, "<60..130> - vertical render FOV"); }
};

} // namespace

//=============================================================================
void RegisterConsoleCommands()
{
    if (g_consoleRegistered || !Console)
        return;

    Console->AddCommand(xr_new<CCC_VR_On>());
    Console->AddCommand(xr_new<CCC_VR_Off>());
    Console->AddCommand(xr_new<CCC_VR_Status>());
    Console->AddCommand(xr_new<CCC_VR_Recenter>());
    Console->AddCommand(xr_new<CCC_VR_Scale>());
    Console->AddCommand(xr_new<CCC_VR_EyeHeight>());
    Console->AddCommand(xr_new<CCC_VR_HudDist>());
    Console->AddCommand(xr_new<CCC_VR_Fov>());

    g_consoleRegistered = true;
}

} // namespace vr
