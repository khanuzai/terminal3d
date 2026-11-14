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

// True if every edge is shared by exactly two triangles. Then the surface has
// no holes, so it has a clear inside and outside, and from outside you can
// never see a triangle's back.
bool isClosed(const Mesh& mesh);

// The volume enclosed by the mesh, with a sign: positive when the triangles
// are wound counter-clockwise seen from outside (normals point out), negative
// when the mesh is inside-out. Only meaningful for closed meshes.
double signedVolume(const Mesh& mesh);

// Reverses the corner order of every triangle, turning the mesh inside-out
// (or back the right way round).
void flipWinding(Mesh& mesh);
