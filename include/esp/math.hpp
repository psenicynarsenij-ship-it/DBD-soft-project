#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <numbers>

namespace esp {
struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a,a)); }
inline Vec3 Normalize(Vec3 a) { const float n=Length(a); return n>0 ? a*(1.0f/n) : Vec3{}; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Radians(float degrees) { return degrees*std::numbers::pi_v<float>/180.0f; }
// Yaw zero faces +Z. Positive yaw turns toward +X about the +Y axis.
inline Vec3 DirectionFromYaw(float radians) { return {std::sin(radians),0,std::cos(radians)}; }
inline Vec3 RotateYaw(Vec3 direction, float radians) {
    const float s=std::sin(radians),c=std::cos(radians);
    return {direction.x*c+direction.z*s,direction.y,-direction.x*s+direction.z*c};
}
// Row-major storage, column vectors, left-handed coordinates, +Y up, +Z forward.
struct Matrix4 { std::array<float,16> m{}; };
inline Matrix4 Multiply(const Matrix4& a, const Matrix4& b) {
    Matrix4 r;
    for (int y=0;y<4;++y) for(int x=0;x<4;++x)
        for(int k=0;k<4;++k) r.m[y*4+x]+=a.m[y*4+k]*b.m[k*4+x];
    return r;
}
inline Matrix4 LookAt(Vec3 eye, Vec3 target) {
    const auto f=Normalize(target-eye);
    const auto r=Normalize(Cross({0,1,0},f));
    const auto u=Cross(f,r);
    return {{r.x,r.y,r.z,-Dot(r,eye), u.x,u.y,u.z,-Dot(u,eye),
             f.x,f.y,f.z,-Dot(f,eye), 0,0,0,1}};
}
inline Matrix4 Perspective(float verticalFovRadians, float aspect, float nearZ, float farZ) {
    const float s=1/std::tan(verticalFovRadians/2);
    return {{s/aspect,0,0,0, 0,s,0,0, 0,0,farZ/(farZ-nearZ),-nearZ*farZ/(farZ-nearZ), 0,0,1,0}};
}
inline std::optional<Vec2> WorldToScreen(Vec3 p, const Matrix4& vp, float width, float height) {
    if (!(width>0 && height>0) || !std::isfinite(width) || !std::isfinite(height)) return {};
    const auto& m=vp.m;
    const float x=m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3];
    const float y=m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7];
    const float z=m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11];
    const float w=m[12]*p.x+m[13]*p.y+m[14]*p.z+m[15];
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(w)||w<=0.0001f||z<0||z>w) return {};
    const Vec2 out{(x/w+1)*width/2,(1-y/w)*height/2};
    if (!std::isfinite(out.x)||!std::isfinite(out.y)) return {};
    return out; // X/Y clipping is left to the renderer (partially visible boxes).
}
}
