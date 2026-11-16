#include "mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
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
    mesh.cornerNormals.clear();
}

void computeCornerNormals(Mesh& mesh, float creaseAngle) {
    const std::size_t faceCount = mesh.triangles.size();

    // Each face's unit normal, and the angle the face makes at each of its
    // three corners. When averaging normals at a point, each face is weighted
    // by its angle there. That makes the result independent of how polygons
    // were split into triangles: a square split in two contributes 45 + 45
    // degrees at a corner the triangles share, the same 90 degrees as the
    // unsplit square. (Weighting by area instead would count that square
    // twice there.)
    std::vector<Vec3> unit(faceCount);
    std::vector<std::array<float, 3>> cornerAngle(faceCount);
    for (std::size_t f = 0; f < faceCount; ++f) {
        const Triangle& t = mesh.triangles[f];
        const Vec3 p[3] = {mesh.vertices[t.a], mesh.vertices[t.b], mesh.vertices[t.c]};
        unit[f] = normalize(cross(p[1] - p[0], p[2] - p[0]));
        for (int k = 0; k < 3; ++k) {
            const Vec3 toNext = normalize(p[(k + 1) % 3] - p[k]);
            const Vec3 toPrev = normalize(p[(k + 2) % 3] - p[k]);
            cornerAngle[f][k] = std::acos(std::clamp(dot(toNext, toPrev), -1.0f, 1.0f));
        }
    }

    // Group vertices by position. Some models store one corner several times
    // (the teapot does, where its curved patches meet). For smoothing, those
    // copies have to count as one point, or every seam shows as a crease.
    std::map<std::array<float, 3>, int> positionIds;
    std::vector<int> positionOf(mesh.vertices.size());
    for (std::size_t v = 0; v < mesh.vertices.size(); ++v) {
        const Vec3& p = mesh.vertices[v];
        const int nextId = static_cast<int>(positionIds.size());
        positionOf[v] = positionIds.emplace(std::array<float, 3>{p.x, p.y, p.z}, nextId).first->second;
    }

    // For each position, the faces that touch it, and at which of their
    // corners (0, 1 or 2), so we can look up the angle there.
    struct FaceCorner {
        int face;
        int corner;
    };
    std::vector<std::vector<FaceCorner>> facesAt(positionIds.size());
    for (std::size_t f = 0; f < faceCount; ++f) {
        const Triangle& t = mesh.triangles[f];
        const int corners[3] = {t.a, t.b, t.c};
        for (int k = 0; k < 3; ++k) {
            facesAt[positionOf[corners[k]]].push_back({static_cast<int>(f), k});
        }
    }

    // Two faces are on the same smooth surface if the angle between their
    // normals is at most creaseAngle, i.e. the cosine (their dot product) is
    // at least cos(creaseAngle).
    const float minCos = std::cos(creaseAngle);

    mesh.cornerNormals.assign(faceCount * 3, Vec3{});
    for (std::size_t f = 0; f < faceCount; ++f) {
        const Triangle& t = mesh.triangles[f];
        const int corners[3] = {t.a, t.b, t.c};
        for (int k = 0; k < 3; ++k) {
            Vec3 sum;
            for (const FaceCorner& fc : facesAt[positionOf[corners[k]]]) {
                if (dot(unit[f], unit[fc.face]) >= minCos) {
                    sum += unit[fc.face] * cornerAngle[fc.face][fc.corner];
                }
            }
            // If everything cancelled out (or the face has no area), fall
            // back to the face's own normal.
            mesh.cornerNormals[3 * f + k] = length(sum) > 0.0f ? normalize(sum) : unit[f];
        }
    }
}
