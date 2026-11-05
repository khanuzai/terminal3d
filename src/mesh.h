#pragma once

#include <vector>

#include "math3d.h"

// One triangle, stored as three indices into Mesh::vertices (0-based).
// Sharing vertices between triangles by index, instead of copying positions,
// is how almost every 3D format and GPU stores geometry.
struct Triangle {
    int a, b, c;
};

// A triangle mesh: a list of points, and triangles that connect them.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};

// Returns a transform that centers the mesh on the origin and scales it to fit
// inside a sphere of radius 1. Models come in wildly different sizes and
// positions; this makes every one of them fit the same camera setup.
Mat4 fitToUnitSphere(const Mesh& mesh);
