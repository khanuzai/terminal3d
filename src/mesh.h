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
