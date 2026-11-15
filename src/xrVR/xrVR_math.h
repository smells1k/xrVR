//-----------------------------------------------------------------------------
// xrVR_math.h - математика VR-модуля. Не зависит от X-Ray: покрыта юнит-тестами
// (tests/vr_math_test.cpp), собирается и на Linux.
//
// Соглашения:
//  * "Мир" модуля = OpenVR-конвенция: метры, +Y вверх, -Z вперёд.
//  * X-Ray: метры, +Y вверх, +Z вперёд, row-vector матрицы.
//  * Ось "право" в OpenVR = +X. В X-Ray при виде от первого лица "право"
//    тоже +X, поэтому переход между базисами - поворот Y на 180 градусов.
//-----------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vrmath
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;
constexpr float kRad2Deg = 180.f / kPi;

//-------------------------------------------------------------------------
struct Vec3
{
    float x{}, y{}, z{};

    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float Length() const { return std::sqrt(Dot(*this)); }

    Vec3 Normalized() const
    {
        const float l = Length();
        return l > 1e-8f ? (*this) * (1.f / l) : Vec3{0.f, 0.f, 1.f};
    }

    Vec3 Cross(const Vec3& o) const
    {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
};

//-------------------------------------------------------------------------
struct Quat
{
    float x{}, y{}, z{}, w{1.f};

    Quat() = default;
    Quat(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    // a * b (порядок: сначала b, затем a)
    Quat operator*(const Quat& b) const
    {
        Quat r;
        r.x = w * b.x + x * b.w + y * b.z - z * b.y;
        r.y = w * b.y - x * b.z + y * b.w + z * b.x;
        r.z = w * b.z + x * b.y - y * b.x + z * b.w;
        r.w = w * b.w - x * b.x - y * b.y - z * b.z;
        return r;
    }

    Quat Conj() const { return {-x, -y, -z, w}; }

    Quat Normalized() const
    {
        const float n = std::sqrt(x * x + y * y + z * z + w * w);
        if (n > 1e-8f)
        {
            const float inv = 1.f / n;
            return {x * inv, y * inv, z * inv, w * inv};
        }
        return {};
    }

    // v' = v + 2w (q x v) + 2 q x (q x v), q = (x,y,z)
    Vec3 Rotate(const Vec3& v) const
    {
        const Vec3 qv{x, y, z};
        const Vec3 t = qv.Cross(v) * 2.f;
        return v + t * w + qv.Cross(t);
    }
};

//-------------------------------------------------------------------------
struct Pose
{
    Vec3 p{};
    Quat q{};
    bool valid{};
};

struct Mat34
{
    float m[12]{}; // 3x4 row-major, трансляция в m[3], m[7], m[11]
};

//-------------------------------------------------------------------------
inline Quat QuatFromYawPitchRoll(float yaw, float pitch, float roll)
{
    const float cy = std::cos(yaw * 0.5f), sy = std::sin(yaw * 0.5f);
    const float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
    const float cr = std::cos(roll * 0.5f), sr = std::sin(roll * 0.5f);

    Quat q;
    q.w = cy * cp * cr + sy * sp * sr;
    q.x = cy * cp * sr - sy * sp * cr;
    q.y = sy * cp * sr + cy * sp * cr;
    q.z = sy * cp * cr - cy * sp * sr;
    return q.Normalized();
}

//-------------------------------------------------------------------------
// Углы камеры: yaw вокруг +Y (положительный => вправо, к +X),
// pitch вверх положительный, roll вокруг взгляда.
//
// Композиция (column-vector): q = Ry(yaw) * Rx(pitch) * Rz(roll),
// так что q.Rotate(0,0,1) = (sin yaw cos pitch, sin pitch, cos yaw cos pitch).
//-------------------------------------------------------------------------
inline Vec3 DirFromYawPitch(float yaw, float pitch)
{
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
}

inline Quat QuatFromYawPitchRollView(float yaw, float pitch, float roll)
{
    const float cy = std::cos(yaw * 0.5f), sy = std::sin(yaw * 0.5f);
    const float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
    const float cr = std::cos(roll * 0.5f), sr = std::sin(roll * 0.5f);

    // Ry * Rx(-pitch) * Rz: положительный pitch => взгляд вверх (+Y).
    Quat qy{0.f, sy, 0.f, cy};
    Quat qx{-sp, 0.f, 0.f, cp};
    Quat qz{0.f, 0.f, sr, cr};
    return (qy * qx * qz).Normalized();
}

inline void ToYawPitchRollView(const Quat& q, float& yaw, float& pitch, float& roll)
{
    const Vec3 fwd = q.Rotate({0.f, 0.f, 1.f});
    const Vec3 up = q.Rotate({0.f, 1.f, 0.f});

    yaw = std::atan2(fwd.x, fwd.z);
    const float sy = fwd.y < -1.f ? -1.f : (fwd.y > 1.f ? 1.f : fwd.y);
    pitch = std::asin(sy);

    // Снимаем yaw и pitch с "вверх": остаётся только Rz(roll).
    // (Ry*Rx)^-1 = Rx(-pitch) * Ry(-yaw)
    const float cy = std::cos(yaw), syw = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);

    // Ry(-yaw) * up
    const Vec3 up_y = {up.x * cy - up.z * syw, up.y, up.x * syw + up.z * cy};
    // Rx(-pitch) * up_y
    const Vec3 up_yp = {up_y.x, up_y.y * cp + up_y.z * sp, -up_y.y * sp + up_y.z * cp};
    roll = std::atan2(-up_yp.x, up_yp.y);
}

//-------------------------------------------------------------------------
// Кватернион из матрицы 3x4 (OpenVR HmdMatrix34_t / матрица базиса X-Ray).
//-------------------------------------------------------------------------
inline Quat QuatFromMat33Rows(const float r[9])
{
    const float m00 = r[0], m01 = r[1], m02 = r[2];
    const float m10 = r[3], m11 = r[4], m12 = r[5];
    const float m20 = r[6], m21 = r[7], m22 = r[8];

    const float tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0.f)
    {
        const float s = std::sqrt(tr + 1.f) * 2.f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float s = std::sqrt(1.f + m00 - m11 - m22) * 2.f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    }
    else if (m11 > m22)
    {
        const float s = std::sqrt(1.f + m11 - m00 - m22) * 2.f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    }
    else
    {
        const float s = std::sqrt(1.f + m22 - m00 - m11) * 2.f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return q.Normalized();
}

//-------------------------------------------------------------------------
// OpenVR-поза (HmdMatrix34_t, +Y up, -Z forward) -> поза X-Ray (+Z forward).
//-------------------------------------------------------------------------
inline Pose PoseFromOpenVR(const float m[12], float world_scale)
{
    Pose out;

    const float r[9] = {m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]};
    const Quat q_ovr = QuatFromMat33Rows(r);

    // Смена базиса: M = diag(1,1,-1) (OpenVR -Z forward -> X-Ray +Z forward).
    // Это отражение, поэтому для вращения: R' = M R M^-1 =>
    // ось отражается, угол меняет знак => q' = (-x, -y, z, w).
    out.q = Quat{-q_ovr.x, -q_ovr.y, q_ovr.z, q_ovr.w}.Normalized();

    // Позиция отражается по Z
    out.p = Vec3(m[3] * world_scale, m[7] * world_scale, -m[11] * world_scale);
    out.valid = true;
    return out;
}

//-------------------------------------------------------------------------
// Матрица «средней точки между глазами»: центр головы.
//  eye_in_head - высота глаз над центром трекинга OpenVR (~0.10 м)
//  eye_height  - рост игрока (метры), задаёт, насколько опустить камеру
//-------------------------------------------------------------------------
inline Pose HeadToViewPose(const Pose& head, float eye_height, float eye_in_head = 0.10f)
{
    Pose v = head;
    const float drop = eye_height - eye_in_head;
    v.p.y -= drop;
    return v;
}

//-------------------------------------------------------------------------
// Смещение глаза относительно центра головы (метры).
//  eye_index: 0 - левый, 1 - правый
//-------------------------------------------------------------------------
inline Vec3 EyeOffset(const Pose& head, int eye_index, float ipd_scale)
{
    const Vec3 right = head.q.Rotate({1.f, 0.f, 0.f});
    const float half_ipd = 0.0315f * ipd_scale; // средний IPD 63 мм
    const float k = (eye_index == 0) ? -0.5f : 0.5f;
    return right * (half_ipd * 2.f * k);
}

//-------------------------------------------------------------------------
// Офф-центр проекция одного глаза: X-Ray умеет только симметричную
// (build_projection_HAT), поэтому:
//  1) рендерим СИММЕТРИЧНЫЙFOV, покрывающий глаз целиком;
//  2) в SteamVR отдаём часть текстуры через VRTextureBounds_t.
//
// Ниже - расчёт этих bounds из tan-углов глаза (l, r, t, b - в долях
// тангенса; отрицательные значения для l и b).
//-------------------------------------------------------------------------
struct EyeBounds
{
    float uMin{}, uMax{}, vMin{}, vMax{};
};

//  renderTanH - тангенс половины горизонтального FOV рендера (симметр.)
//  renderTanV - тангенс половины вертикального FOV рендера
inline EyeBounds ComputeEyeBounds(float tanL, float tanR, float tanT, float tanB, float renderTanH, float renderTanV)
{
    EyeBounds b{};
    if (renderTanH <= 0.f || renderTanV <= 0.f)
    {
        b = {0.f, 1.f, 0.f, 1.f};
        return b;
    }

    // NDC = tan_x / renderTanH; UV = 0.5 + 0.5 * NDC
    b.uMin = 0.5f + 0.5f * (tanL / renderTanH);
    b.uMax = 0.5f + 0.5f * (tanR / renderTanH);

    // V (SteamVR: 0 = верх)
    b.vMin = 0.5f - 0.5f * (tanT / renderTanV);
    b.vMax = 0.5f - 0.5f * (tanB / renderTanV);

    // Зажим в [0,1] на случай мелких погрешностей
    auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
    b.uMin = clamp01(b.uMin);
    b.uMax = clamp01(b.uMax);
    b.vMin = clamp01(b.vMin);
    b.vMax = clamp01(b.vMax);
    return b;
}

//-------------------------------------------------------------------------
// Проверка: покрывает ли симметричный рендер глаз целиком.
//-------------------------------------------------------------------------
inline bool BoundsValid(const EyeBounds& b)
{
    return (b.uMax - b.uMin) > 0.05f && (b.vMax - b.vMin) > 0.05f;
}

//-------------------------------------------------------------------------
// Рекомендуемые tan-углы рендера для глаза (симметричный «обнимающий» FOV).
//  aspect - соотношение сторон экрана/таргета рендера.
//-------------------------------------------------------------------------
struct RenderFov
{
    float tanH{}, tanV{};
};

inline RenderFov ComputeRenderFov(float tanL, float tanR, float tanT, float tanB, float aspect, float minTan = 0.30f)
{
    // Нужно покрыть горизонталь и вертикаль, уложившись в аспект.
    const float needH = std::max({std::fabs(tanL), std::fabs(tanR), minTan});
    const float needV = std::max({std::fabs(tanT), std::fabs(tanB), minTan});

    RenderFov f{};
    // Начинаем с горизонтали и растягиваем вертикаль
    if (needH / needV >= aspect)
    {
        f.tanH = needH;
        f.tanV = needH / aspect;
    }
    else
    {
        f.tanV = needV;
        f.tanH = needV * aspect;
    }
    return f;
}

//-------------------------------------------------------------------------
// Интерполяция позы (для позного предсказания)
//-------------------------------------------------------------------------
inline Pose LerpPose(const Pose& a, const Pose& b, float t)
{
    Pose r;
    r.p = a.p + (b.p - a.p) * t;

    Quat qa = a.q, qb = b.q;
    float d = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
    if (d < 0.f)
    {
        qb = {-qb.x, -qb.y, -qb.z, -qb.w};
        d = -d;
    }
    const float s = (d > 0.9995f) ? 1.f : std::acos(d);
    const float sn = std::sin(s);
    float w1 = 1.f - t, w2 = t;
    if (sn > 1e-6f)
    {
        w1 = std::sin((1.f - t) * s) / sn;
        w2 = std::sin(t * s) / sn;
    }
    r.q = Quat{qa.x * w1 + qb.x * w2, qa.y * w1 + qb.y * w2, qa.z * w1 + qb.z * w2, qa.w * w1 + qb.w * w2}.Normalized();
    r.valid = a.valid && b.valid;
    return r;
}

//-------------------------------------------------------------------------
// Угловая разница yaw с учётом перехода через 0/360
//-------------------------------------------------------------------------
inline float YawDelta(float current, float previous)
{
    float d = current - previous;
    while (d > 180.f) d -= 360.f;
    while (d < -180.f) d += 360.f;
    return d;
}

} // namespace vrmath
