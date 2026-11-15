//-----------------------------------------------------------------------------
// vr_math_test.cpp - юнит-тесты математики VR-модуля (Linux/Windows, без X-Ray)
// Сборка: g++ -std=c++17 -O2 -Wall -Wextra vr_math_test.cpp -o vr_math_test
//-----------------------------------------------------------------------------
#include "../src/xrVR/xrVR_math.h"

#include <cmath>
#include <cstdio>

using namespace vrmath;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK_NEAR(a, b, eps, what)                                                        \
    do {                                                                                    \
        ++g_checks;                                                                         \
        const float _d = std::fabs((float)(a) - (float)(b));                                 \
        if (_d > (eps)) {                                                                    \
            ++g_failures;                                                                    \
            std::printf("FAIL %s: %.6f vs %.6f (d=%.6f)\n", what, (double)(a), (double)(b),  \
                        (double)_d);                                                         \
        }                                                                                   \
    } while (0)

#define CHECK_TRUE(cond, what)                                        \
    do {                                                              \
        ++g_checks;                                                   \
        if (!(cond)) { ++g_failures; std::printf("FAIL %s\n", what); } \
    } while (0)

// OpenVR-матрица поворота вокруг +Y (row-major 3x4)
static void MakeOvrYawMatrix(float yaw_deg, float m[12])
{
    const float y = yaw_deg * kDeg2Rad;
    const float c = std::cos(y), s = std::sin(y);

    m[0] = c;   m[1] = 0.f; m[2] = s;   m[3] = 0.f;
    m[4] = 0.f; m[5] = 1.f; m[6] = 0.f; m[7] = 1.6f; // глаза на 1.6 м
    m[8] = -s;  m[9] = 0.f; m[10] = c;  m[11] = 0.f;
}

static void TestPoseConversion()
{
    float m[12];

    MakeOvrYawMatrix(0.f, m);
    Pose p = PoseFromOpenVR(m, 1.f);
    Vec3 fwd = p.q.Rotate({0.f, 0.f, 1.f});
    CHECK_NEAR(fwd.x, 0.f, 1e-5f, "yaw0 forward.x");
    CHECK_NEAR(fwd.z, 1.f, 1e-5f, "yaw0 forward.z");
    CHECK_NEAR(p.p.y, 1.6f, 1e-5f, "head height");

    MakeOvrYawMatrix(90.f, m);
    p = PoseFromOpenVR(m, 1.f);
    fwd = p.q.Rotate({0.f, 0.f, 1.f});
    // OpenVR: положительный yaw = поворот налево (CCW). В X-Ray после
    // конвертации взгляд должен смотреть в -X.
    CHECK_NEAR(fwd.x, -1.f, 1e-5f, "openvr yaw90 -> xray forward.x");
    CHECK_NEAR(fwd.z, 0.f, 1e-5f, "openvr yaw90 -> xray forward.z");
}

static void TestYawPitchRoll()
{
    Quat q = QuatFromYawPitchRollView(0.5f, -0.2f, 0.f);
    float yaw, pitch, roll;
    ToYawPitchRollView(q, yaw, pitch, roll);
    CHECK_NEAR(yaw, 0.5f, 1e-4f, "ypr yaw");
    CHECK_NEAR(pitch, -0.2f, 1e-4f, "ypr pitch");
    CHECK_NEAR(roll, 0.f, 1e-4f, "ypr roll");

    q = QuatFromYawPitchRollView(0.f, 0.f, 0.3f);
    ToYawPitchRollView(q, yaw, pitch, roll);
    CHECK_NEAR(yaw, 0.f, 1e-4f, "roll-only yaw");
    CHECK_NEAR(pitch, 0.f, 1e-4f, "roll-only pitch");
    CHECK_NEAR(roll, 0.3f, 1e-4f, "roll-only roll");
}

static void TestEyeOffset()
{
    Pose head;
    head.valid = true;

    Vec3 l = EyeOffset(head, 0, 1.f);
    Vec3 r = EyeOffset(head, 1, 1.f);

    CHECK_TRUE(l.x < 0.f, "left eye on -X");
    CHECK_TRUE(r.x > 0.f, "right eye on +X");
    CHECK_NEAR(std::fabs(l.x), 0.0315f, 1e-5f, "ipd half");
    CHECK_NEAR(l.y, 0.f, 1e-6f, "left eye y");
    CHECK_NEAR(l.z, 0.f, 1e-6f, "left eye z");

    head.q = QuatFromYawPitchRollView(90.f * kDeg2Rad, 0.f, 0.f);
    l = EyeOffset(head, 0, 1.f);
    CHECK_NEAR(l.x, 0.f, 1e-5f, "rot left eye x");
    CHECK_NEAR(l.z, 0.0315f, 1e-5f, "rot left eye z");
}

static void TestEyeBounds()
{
    const float tanL = -1.10f, tanR = 1.05f, tanT = 0.92f, tanB = -0.95f;
    RenderFov fov = ComputeRenderFov(tanL, tanR, tanT, tanB, 1.2f);
    EyeBounds b = ComputeEyeBounds(tanL, tanR, tanT, tanB, fov.tanH, fov.tanV);

    CHECK_TRUE(BoundsValid(b), "bounds valid");
    CHECK_TRUE(b.uMin >= 0.f && b.uMin <= 1.f, "uMin in range");
    CHECK_TRUE(b.uMax >= 0.f && b.uMax <= 1.f, "uMax in range");
    CHECK_TRUE(b.vMin >= 0.f && b.vMin <= 1.f, "vMin in range");
    CHECK_TRUE(b.vMax >= 0.f && b.vMax <= 1.f, "vMax in range");

    b = ComputeEyeBounds(-1.f, 1.f, 1.f, -1.f, 1.f, 1.f);
    CHECK_NEAR(b.uMin, 0.f, 1e-5f, "sym uMin");
    CHECK_NEAR(b.uMax, 1.f, 1e-5f, "sym uMax");
    CHECK_NEAR(b.vMin, 0.f, 1e-5f, "sym vMin");
    CHECK_NEAR(b.vMax, 1.f, 1e-5f, "sym vMax");

    b = ComputeEyeBounds(-0.5f, 1.5f, 1.f, -1.f, 2.f, 1.f);
    CHECK_NEAR(b.uMin, 0.375f, 1e-5f, "asym uMin");
    CHECK_NEAR(b.uMax, 0.875f, 1e-5f, "asym uMax");
}

static void TestHeadToViewPose()
{
    Pose head;
    head.p = {0.f, 1.8f, 0.f};
    head.valid = true;

    Pose v = HeadToViewPose(head, 1.8f);
    CHECK_NEAR(v.p.y, 0.1f, 1e-5f, "view drop");

    v = HeadToViewPose(head, 1.5f);
    CHECK_NEAR(v.p.y, 0.4f, 1e-5f, "view drop short player");
}

static void TestYawDelta()
{
    CHECK_NEAR(YawDelta(5.f, 355.f), 10.f, 1e-5f, "wrap forward");
    CHECK_NEAR(YawDelta(355.f, 5.f), -10.f, 1e-5f, "wrap backward");
    CHECK_NEAR(YawDelta(90.f, 45.f), 45.f, 1e-5f, "plain");
}

static void TestLerpPose()
{
    Pose a, b;
    a.p = {0.f, 0.f, 0.f};
    a.q = QuatFromYawPitchRollView(0.f, 0.f, 0.f);
    a.valid = true;
    b.p = {10.f, 0.f, 0.f};
    b.q = QuatFromYawPitchRollView(kPi * 0.5f, 0.f, 0.f);
    b.valid = true;

    Pose m = LerpPose(a, b, 0.5f);
    CHECK_NEAR(m.p.x, 5.f, 1e-5f, "lerp pos");
    float yaw, pitch, roll;
    ToYawPitchRollView(m.q, yaw, pitch, roll);
    CHECK_NEAR(yaw, kPi * 0.25f, 1e-4f, "lerp yaw");
}

int main()
{
    std::printf("=== xrVR math unit tests ===\n");
    TestPoseConversion();
    TestYawPitchRoll();
    TestEyeOffset();
    TestEyeBounds();
    TestHeadToViewPose();
    TestYawDelta();
    TestLerpPose();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
