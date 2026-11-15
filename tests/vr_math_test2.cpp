// Дополнительные тесты: FOV рендера и bounds для реальных шлемов.
#include "../src/xrVR/xrVR_math.h"
#include <cmath>
#include <cstdio>

using namespace vrmath;
static int fails = 0, checks = 0;

#define CK(cond, what) do{ ++checks; if(!(cond)){ ++fails; std::printf("FAIL %s\n", what);} }while(0)
#define CKN(a,b,eps,what) do{ ++checks; if(std::fabs((a)-(b))>(eps)){ ++fails; std::printf("FAIL %s: %.6f vs %.6f\n", what,(double)(a),(double)(b));} }while(0)

struct EyeSpec { float l, r, t, b; const char* name; };

void CheckHeadset(const EyeSpec& e, float aspect)
{
    RenderFov fov = ComputeRenderFov(e.l, e.r, e.t, e.b, aspect);
    EyeBounds b = ComputeEyeBounds(e.l, e.r, e.t, e.b, fov.tanH, fov.tanV);

    std::printf("  %s: render TanH=%.2f TanV=%.2f (FOV %.1fx%.1f deg) bounds=[%.3f..%.3f, %.3f..%.3f]\n",
                e.name, fov.tanH, fov.tanV,
                2.f * std::atan(fov.tanH) * kRad2Deg, 2.f * std::atan(fov.tanV) * kRad2Deg,
                b.uMin, b.uMax, b.vMin, b.vMax);

    CK(BoundsValid(b), "bounds valid");
    CK(b.uMin >= -1e-6f && b.uMax <= 1.f + 1e-6f, "u bounds");
    CK(b.vMin >= -1e-6f && b.vMax <= 1.f + 1e-6f, "v bounds");

    // Ключевая проверка: видимая область глаза покрыта полностью
    CK(fov.tanH >= std::fabs(e.l) - 1e-4f && fov.tanH >= std::fabs(e.r) - 1e-4f, "H covered");
    CK(fov.tanV >= std::fabs(e.t) - 1e-4f && fov.tanV >= std::fabs(e.b) - 1e-4f, "V covered");

    // Симметричность относительно центра текстуры у Valve-подобных шлемов
    CK((b.uMax - b.uMin) > 0.4f, "h size sane");
    CK((b.vMax - b.vMin) > 0.4f, "v size sane");
}

int main()
{
    std::printf("=== Eye bounds / FOV tests ===\n");

    // Valve Index (данные из OpenVR: ~130x110 суммарно на глаз)
    CheckHeadset({-1.10f, 1.05f, 0.95f, -0.97f, "Valve Index"}, 1.0f);

    // Pico Neo 3 (приблизительно, ~98 deg)
    CheckHeadset({-0.82f, 0.80f, 0.87f, -0.89f, "Pico Neo 3"}, 1.0f);

    // Старенький Rift CV1
    CheckHeadset({-0.90f, 0.93f, 0.89f, -0.92f, "Rift CV1"}, 1.3f);

    // HTC Vive
    CheckHeadset({-1.00f, 0.95f, 0.93f, -0.98f, "HTC Vive"}, 1.3f);

    // Квадратный аспект: рендер должен растянуться
    RenderFov fov = ComputeRenderFov(-1.0f, 1.0f, 1.0f, -1.0f, 1.0f);
    CKN(fov.tanH, 1.0f, 1e-5f, "square tanH");
    CKN(fov.tanV, 1.0f, 1e-5f, "square tanV");

    // Широкий аспект (16:9): чтобы покрыть вертикаль, горизонталь расширяется
    fov = ComputeRenderFov(-1.0f, 1.0f, 1.0f, -1.0f, 16.f/9.f);
    CKN(fov.tanH, 16.f/9.f, 1e-4f, "169 tanH");
    CKN(fov.tanV, 1.0f, 1e-5f, "169 tanV");

    // Узкий глаз (минимальный случай)
    EyeBounds b = ComputeEyeBounds(-0.2f, 0.2f, 0.2f, -0.2f, 1.f, 1.f);
    CKN(b.uMin, 0.4f, 1e-5f, "narrow uMin");
    CKN(b.uMax, 0.6f, 1e-5f, "narrow uMax");

    std::printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
