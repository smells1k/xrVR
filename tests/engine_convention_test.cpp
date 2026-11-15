//-----------------------------------------------------------------------------
// engine_convention_test.cpp
//
// ЗАЩИЩАЕТ от регрессий в соглашениях между vrmath и X-Ray.
// Сверено с реальными формулами движка:
//   Q.rotationYawPitchRoll(roll, yaw, pitch); mR.rotation(Q); mR.transpose();
//   vDirection = mR.k;  vNormal = mR.j;
// (xrGame/CameraFirstEye.cpp:33-38, xrCore/vector.h:269, _quaternion.h:299)
//
// Проверяемые факты (получены численным прогоном настоящих формул):
//   движок  yaw=+30     -> dir=(+0.500, 0, 0.866)
//   движок  pitch=+30   -> dir=(0, -0.500, 0.866)   <-- ВНИЗ
//   движок  roll=+20    -> up =(-0.342, 0.940, 0)
//
// Вывод: yaw и roll у vrmath и X-Ray совпадают, а pitch ПРОТИВОПОЛОЖЕН.
// Поэтому patch_02_camera.py пишет "pitch = -v_pitch".
//-----------------------------------------------------------------------------
#include "xrVR_math.h"
#include <cmath>
#include <cstdio>

static int g_checks = 0;
static int g_fails = 0;

static void expect_near(const char* what, float got, float want, float eps = 2e-3f)
{
    ++g_checks;
    if (std::fabs(got - want) > eps)
    {
        ++g_fails;
        std::printf("  FAIL %-46s got %8.4f want %8.4f\n", what, got, want);
    }
}

int main()
{
    const float D = vrmath::kDeg2Rad;
    std::printf("=== Соглашения vrmath vs X-Ray (CameraFirstEye) ===\n");

    // --- yaw: оба соглашения дают +X при положительном угле ---------------
    {
        vrmath::Quat q = vrmath::QuatFromYawPitchRollView(30.f * D, 0.f, 0.f);
        vrmath::Vec3 d = q.Rotate({0.f, 0.f, 1.f});
        std::printf("yaw=+30   vrmath dir=(%6.3f,%6.3f,%6.3f)  X-Ray=( 0.500, 0.000, 0.866)\n", d.x, d.y, d.z);
        expect_near("yaw+30 dir.x", d.x, 0.5f);
        expect_near("yaw+30 dir.z", d.z, 0.866f);
    }

    // --- pitch: ЗНАК ПРОТИВОПОЛОЖЕН -------------------------------------
    {
        vrmath::Quat q = vrmath::QuatFromYawPitchRollView(0.f, 30.f * D, 0.f);
        vrmath::Vec3 d = q.Rotate({0.f, 0.f, 1.f});
        std::printf("pitch=+30 vrmath dir.y=%6.3f  X-Ray dir.y=-0.500  => нужна инверсия\n", d.y);
        expect_near("pitch+30 vrmath смотрит ВВЕРХ", d.y, 0.5f);

        // Инверсия (как в патче) должна дать движковый результат
        vrmath::Quat q2 = vrmath::QuatFromYawPitchRollView(0.f, -30.f * D, 0.f);
        vrmath::Vec3 d2 = q2.Rotate({0.f, 0.f, 1.f});
        expect_near("pitch=-30 (после инверсии) dir.y", d2.y, -0.5f);
    }

    // --- roll: соглашения совпадают -------------------------------------
    {
        vrmath::Quat q = vrmath::QuatFromYawPitchRollView(0.f, 0.f, 20.f * D);
        vrmath::Vec3 up = q.Rotate({0.f, 1.f, 0.f});
        std::printf("roll=+20  vrmath up=(%6.3f,%6.3f,%6.3f)  X-Ray=(-0.342, 0.940, 0.000)\n", up.x, up.y, up.z);
        expect_near("roll+20 up.x", up.x, -0.342f);
        expect_near("roll+20 up.y", up.y, 0.940f);
    }

    // --- комбинация: yaw=+30, pitch=+20 (движок) =========================
    {
        // Движок: dir=(0.470,-0.342,0.814). У нас: yaw=+30, pitch=-20.
        vrmath::Quat q = vrmath::QuatFromYawPitchRollView(30.f * D, -20.f * D, 0.f);
        vrmath::Vec3 d = q.Rotate({0.f, 0.f, 1.f});
        std::printf("yaw30/pitch-20 vrmath=(%6.3f,%6.3f,%6.3f) X-Ray=( 0.470,-0.342, 0.814)\n", d.x, d.y, d.z);
        expect_near("combo dir.x", d.x, 0.470f);
        expect_near("combo dir.y", d.y, -0.342f);
        expect_near("combo dir.z", d.z, 0.814f);
    }

    std::printf("\n%d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
