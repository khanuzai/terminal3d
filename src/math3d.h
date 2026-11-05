#pragma once

#include <cmath>

// A 3D vector of floats. Used for positions (points in space) and
// directions (normals, light direction, etc.).
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

// Lets us write `2.0f * v` as well as `v * 2.0f`.
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

// Dot product: |a||b|cos(angle). Positive when a and b point the same way,
// zero when perpendicular. We'll use it for lighting.
inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Cross product: a vector perpendicular to both a and b. We'll use it to
// compute a triangle's surface normal from two of its edges.
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

// Returns a vector with the same direction and length 1.
// A zero vector has no direction, so it is returned unchanged.
inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len == 0.0f) return v;
    return v * (1.0f / len);
}
