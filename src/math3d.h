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

// A 4D vector, used for "homogeneous coordinates". A 3D point (x, y, z) is
// written as (x, y, z, 1). The extra w component is what lets a single 4x4
// matrix express translation and perspective as well as rotation and scale.
struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4() = default;
    constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
};

// A 4x4 matrix stored as m[row][col]. Matrices multiply column vectors on the
// right (v' = M * v), like in math textbooks, so in `A * B * v` the
// transform B is applied first, then A.
struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity() {
        Mat4 r;
        for (int i = 0; i < 4; ++i) r.m[i][i] = 1.0f;
        return r;
    }

    // Moves points by t.
    static Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.m[0][3] = t.x;
        r.m[1][3] = t.y;
        r.m[2][3] = t.z;
        return r;
    }

    // Scales points uniformly around the origin.
    static Mat4 scale(float s) {
        Mat4 r = identity();
        r.m[0][0] = s;
        r.m[1][1] = s;
        r.m[2][2] = s;
        return r;
    }

    // Rotations around each axis, counter-clockwise when looking from the
    // positive end of the axis toward the origin (the right-hand rule).
    static Mat4 rotationX(float radians) {
        const float c = std::cos(radians), s = std::sin(radians);
        Mat4 r = identity();
        r.m[1][1] = c;  r.m[1][2] = -s;
        r.m[2][1] = s;  r.m[2][2] = c;
        return r;
    }

    static Mat4 rotationY(float radians) {
        const float c = std::cos(radians), s = std::sin(radians);
        Mat4 r = identity();
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    static Mat4 rotationZ(float radians) {
        const float c = std::cos(radians), s = std::sin(radians);
        Mat4 r = identity();
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
        return r;
    }

    // Perspective projection for a camera at the origin looking down -z.
    //   fovY:   vertical field of view, in radians
    //   aspect: visible width / visible height
    //   zNear, zFar: distances to the near and far clipping planes
    // It places -z (the distance in front of the camera) into w, so the
    // divide by w that follows shrinks distant things: that's perspective.
    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
        const float f = 1.0f / std::tan(fovY / 2.0f);
        Mat4 r;
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = (zFar + zNear) / (zNear - zFar);
        r.m[2][3] = (2.0f * zFar * zNear) / (zNear - zFar);
        r.m[3][2] = -1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += m[row][k] * o.m[k][col];
                r.m[row][col] = sum;
            }
        }
        return r;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w,
        };
    }
};
