#include "mesh.h"

#include <algorithm>

Mat4 fitToUnitSphere(const Mesh& mesh) {
    if (mesh.vertices.empty()) return Mat4::identity();

    // Find the axis-aligned bounding box, and use its center as the model's center.
    Vec3 lo = mesh.vertices[0];
    Vec3 hi = mesh.vertices[0];
    for (const Vec3& v : mesh.vertices) {
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    const Vec3 center = (lo + hi) * 0.5f;

    // The radius is the distance from the center to the farthest vertex. Using
    // a sphere instead of the box means the model still fits after any rotation.
    float radius = 0.0f;
    for (const Vec3& v : mesh.vertices) {
        radius = std::max(radius, length(v - center));
    }
    if (radius == 0.0f) radius = 1.0f; // all vertices at one point; don't divide by 0

    // Read right to left: first move the center to the origin, then shrink.
    return Mat4::scale(1.0f / radius) * Mat4::translation(-center);
}
