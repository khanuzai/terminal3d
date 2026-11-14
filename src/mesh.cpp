#include "mesh.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>

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

bool isClosed(const Mesh& mesh) {
    if (mesh.triangles.empty()) return false;

    // Count how many triangles use each edge. An edge is identified by its two
    // vertex indices, smaller one first so that a->b and b->a count as the same
    // edge, packed into one 64-bit number to use as the map key.
    std::unordered_map<std::uint64_t, int> edgeUses;
    edgeUses.reserve(mesh.triangles.size() * 3);
    auto addEdge = [&](int i, int j) {
        const auto lo = static_cast<std::uint64_t>(std::min(i, j));
        const auto hi = static_cast<std::uint64_t>(std::max(i, j));
        ++edgeUses[(lo << 32) | hi];
    };
    for (const Triangle& t : mesh.triangles) {
        addEdge(t.a, t.b);
        addEdge(t.b, t.c);
        addEdge(t.c, t.a);
    }

    for (const auto& entry : edgeUses) {
        if (entry.second != 2) return false; // a hole's rim (1) or a fin (3+)
    }
    return true;
}

double signedVolume(const Mesh& mesh) {
    // Each triangle forms a tetrahedron (a pyramid with a triangular base)
    // with the origin, whose signed volume is dot(a, cross(b, c)) / 6. The
    // sign depends on whether the triangle faces away from the origin or
    // toward it. Added over a closed surface, the parts outside the shape
    // cancel out and what's left is exactly the enclosed volume, wherever the
    // origin is. If the mesh is inside-out, every term flips sign, so the
    // total comes out negative.
    double sum = 0.0;
    for (const Triangle& t : mesh.triangles) {
        const Vec3& a = mesh.vertices[t.a];
        const Vec3& b = mesh.vertices[t.b];
        const Vec3& c = mesh.vertices[t.c];
        sum += static_cast<double>(dot(a, cross(b, c)));
    }
    return sum / 6.0;
}

void flipWinding(Mesh& mesh) {
    // Swapping two corners reverses the direction around the triangle.
    for (Triangle& t : mesh.triangles) std::swap(t.b, t.c);
}
