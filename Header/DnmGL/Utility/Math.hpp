#pragma once

#include "DnmGL/Utility/Counter.hpp"
#include <array>
#include <cstdint>
#include <cmath>

//i think u should be use or read glm
namespace DnmGL {
    #define VEC_OPS2(T) \
        constexpr auto operator<=>(const T&) const = default; \
        constexpr T operator+(T o) const { return {x + o.x, y + o.y}; } \
        constexpr T operator-(T o) const { return {x - o.x, y - o.y}; } \
        constexpr T operator*(T o) const { return {x * o.x, y * o.y}; } \
        constexpr T operator/(T o) const { return {x / o.x, y / o.y}; } \
        constexpr T& operator+=(T o){ x+=o.x; y+=o.y; return *this; } \
        constexpr T& operator-=(T o){ x-=o.x; y-=o.y; return *this; } \
        constexpr T& operator*=(T o){ x*=o.x; y*=o.y; return *this; } \
        constexpr T& operator/=(T o){ x/=o.x; y/=o.y; return *this; }

    #define VEC_OPS3(T) \
        constexpr auto operator<=>(const T&) const = default; \
        constexpr T operator+(T u) const { return {x + u.x, y + u.y, z + u.z}; } \
        constexpr T operator-(T u) const { return {x - u.x, y - u.y, z - u.z}; } \
        constexpr T operator*(T u) const { return {x * u.x, y * u.y, z * u.z}; } \
        constexpr T operator/(T u) const { return {x / u.x, y / u.y, z / u.z}; } \
        constexpr T& operator+=(T o){ x+=o.x; y+=o.y; z+=o.z; return *this; } \
        constexpr T& operator-=(T o){ x-=o.x; y-=o.y; z-=o.z; return *this; } \
        constexpr T& operator*=(T o){ x*=o.x; y*=o.y; z*=o.z; return *this; } \
        constexpr T& operator/=(T o){ x/=o.x; y/=o.y; z/=o.z; return *this; }

    #define VEC_OPS4(T) \
        constexpr auto operator<=>(const T&) const = default; \
        constexpr T operator+(T o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; } \
        constexpr T operator-(T o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; } \
        constexpr T operator*(T o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; } \
        constexpr T operator/(T o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; } \
        constexpr T& operator+=(T o){ x+=o.x; y+=o.y; z+=o.z; w+=o.w; return *this; } \
        constexpr T& operator-=(T o){ x-=o.x; y-=o.y; z-=o.z; w-=o.w; return *this; } \
        constexpr T& operator*=(T o){ x*=o.x; y*=o.y; z*=o.z; w*=o.w; return *this; } \
        constexpr T& operator/=(T o){ x/=o.x; y/=o.y; z/=o.z; w/=o.w; return *this; }

    struct Uint2 {
        constexpr Uint2() = default;
        constexpr Uint2(uint32_t i) : x(i), y(i) {}
        constexpr Uint2(uint32_t x, uint32_t y) : x(x), y(y) {}

        uint32_t x, y;

        uint32_t& operator[](uint32_t i) {
            return i == 0 ? x : y;
        }

        VEC_OPS2(Uint2);
    };

    struct Uint3 {
        constexpr Uint3() = default;
        constexpr Uint3(uint32_t i) : x(i), y(i), z(i) {}
        constexpr Uint3(Uint2 xy, uint32_t z) : x(xy.x), y(xy.y), z(z) {}
        constexpr Uint3(uint32_t x, uint32_t y, uint32_t z) : x(x), y(y), z(z) {}
        uint32_t x, y, z;

        uint32_t& operator[](uint32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
            }
            return z;
        }

        VEC_OPS3(Uint3);
    };

    struct Uint4 {
        constexpr Uint4() = default;
        constexpr Uint4(uint32_t i) : x(i), y(i), z(i), w(i) {}
        constexpr Uint4(Uint2 xy, Uint2 zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
        constexpr Uint4(Uint3 xyz, uint32_t w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
        constexpr Uint4(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
            : x(x), y(y), z(z), w(w) {}
        uint32_t x, y, z, w;

        uint32_t& operator[](uint32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
                case 2: return z;
            }
            return w;
        }

        VEC_OPS4(Uint4);
    };

    struct Float2 {
        constexpr Float2() = default;
        constexpr Float2(float i) : x(i), y(i) {}
        constexpr Float2(float x, float y) : x(x), y(y) {}
        float x, y;

        float& operator[](uint32_t i) {
            return i == 0 ? x : y;
        }

        VEC_OPS2(Float2);
    };

    struct Float3 {
        constexpr Float3() = default;
        constexpr Float3(float i) : x(i), y(i), z(i) {}
        constexpr Float3(Float2 xy, float z) : x(xy.x), y(xy.y), z(z) {}
        constexpr Float3(float x, float y, float z) : x(x), y(y), z(z) {}
        float x, y, z;

        float& operator[](uint32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
            }
            return z;
        }

        VEC_OPS3(Float3);
    };

    struct Float4 {
        constexpr Float4() = default;
        constexpr Float4(float i) : x(i), y(i), z(i), w(i) {}
        constexpr Float4(Float2 xy, Float2 zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
        constexpr Float4(Float3 xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
        constexpr Float4(float x, float y, float z, float w)
            : x(x), y(y), z(z), w(w) {}
        float x, y, z, w;

        float& operator[](uint32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
                case 2: return z;
            }
            return w;
        }

        const float& operator[](uint32_t i) const {
            switch (i) {
                case 0: return x;
                case 1: return y;
                case 2: return z;
            }
            return w;
        }

        VEC_OPS4(Float4);
    };

    constexpr float DotProduct(Float4 v0, Float4 v1) {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + v0.w * v1.w;
    }

    constexpr float DotProduct(Float3 v0, Float3 v1) {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    }

    constexpr float DotProduct(Float2 v0, Float2 v1) {
        return v0.x * v1.x + v0.y * v1.y;
    }

    //colomn major
    struct Mat4x4 {
        Mat4x4() = default;
        Mat4x4(float s) : column({
            Float4(s, 0, 0, 0), 
            Float4(0, s, 0, 0), 
            Float4(0, 0, s, 0), 
            Float4(0, 0, 0, s)}) {}
        Mat4x4(const std::array<Float4, 4>& a) : column(a) {}

        constexpr Mat4x4 operator+(Mat4x4 o) const { return std::array{column[0] + o.column[0], column[1] + o.column[1], column[2] + o.column[2], column[3] + o.column[3]}; }
        constexpr Mat4x4 operator-(Mat4x4 o) const { return std::array{column[0] - o.column[0], column[1] - o.column[1], column[2] - o.column[2], column[3] - o.column[3]}; }
        constexpr Mat4x4 operator*(Mat4x4 o) const;
        constexpr Mat4x4 operator*(Float4 o) const;

        constexpr Mat4x4& operator+=(Mat4x4 o){ column[0]+=o.column[0]; column[1]+=o.column[1]; column[2]+=o.column[2]; column[3]+=o.column[3]; return *this; }
        constexpr Mat4x4& operator-=(Mat4x4 o){ column[0]-=o.column[0]; column[1]-=o.column[1]; column[2]-=o.column[2]; column[3]-=o.column[3]; return *this; }

        Float4& operator[](uint32_t i) {
            return column[i];
        }
        std::array<Float4, 4> column{};
    };

    constexpr Mat4x4 Mat4x4::operator*(Mat4x4 other) const { 
        Mat4x4 out;
        for (const auto c : Counter(4))
            for (const auto r : Counter(4))
                out[c][r] =
                    column[0][r] * other[c][0] +
                    column[1][r] * other[c][1] +
                    column[2][r] * other[c][2] +
                    column[3][r] * other[c][3];
        return out;
    }

    constexpr Mat4x4 Mat4x4::operator*(Float4 other) const { 
        Mat4x4 out;
        for (const auto i : Counter(4)) {
            out[i] = DotProduct(column[i], other);
        }
        return out;
    }

    constexpr Mat4x4 LookUp(Float3 right, Float3 up, Float3 dir, Float3 pos) {
        return Mat4x4(
            {Float4(right.x, up.x, dir.x, 0),
            Float4(right.y, up.y, dir.y, 0),
            Float4(right.z, up.z, dir.z, 0),
            Float4(0, 0, 0, 1)}
        ) *
        Mat4x4(
            {Float4(1, up.x, dir.x, -pos.x),
            Float4(right.y, 1, dir.y, -pos.y),
            Float4(right.z, up.z, 1, -pos.z),
            Float4(0, 0, 0, 1)}
        );
    }

    // Derived from OpenGL Mathematics (GLM)
    // Source: glm/ext/matrix_clip_space.inl (orthoLH_ZO)
    // License: MIT / Happy Bunny
    // https://github.com/g-truc/glm
    constexpr Mat4x4 Ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
        Mat4x4 out(1);
        out[0][0] = 2.f / (right - left);
        out[1][1] = 2.f / (top - bottom);
        out[2][2] = 1.f / (zFar - zNear);
        out[3][0] = - (right + left) / (right - left);
        out[3][1] = - (top + bottom) / (top - bottom);
        out[3][2] = - zNear / (zFar - zNear);
        return out;
    }

    // Derived from OpenGL Mathematics (GLM)
    // Source: glm/ext/matrix_clip_space.inl (perspectiveLH_ZO)
    // License: MIT / Happy Bunny
    // https://github.com/g-truc/glm
    constexpr Mat4x4 Perspective(float fovy, float aspect, float zNear, float zFar) {
		const float tanHalfFovy = tan(fovy / 2.f);

		Mat4x4 Result{};
        Result[0][0] = 1.f / (aspect * tanHalfFovy);
		Result[1][1] = 1.f / (tanHalfFovy);
		Result[2][2] = zFar / (zFar - zNear);
		Result[2][3] = 1.f;
		Result[3][2] = -(zFar * zNear) / (zFar - zNear);
		return Result;
    }
}