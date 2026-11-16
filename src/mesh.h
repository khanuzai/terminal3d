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

    // Smooth-shading normals, one per triangle corner: corner k (0, 1, 2 for
    // a, b, c) of triangle i is at index 3 * i + k. Stored per corner, not per
    // vertex, so that a vertex on a sharp edge can have a different normal on
    // each side of it. Empty until computeCornerNormals() is called.
    std::vector<Vec3> cornerNormals;
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
// (or back the right way round). Clears cornerNormals, which would now point
// the wrong way; compute them after deciding the winding.
void flipWinding(Mesh& mesh);

// Fills mesh.cornerNormals for smooth shading. Each corner's normal is the
// average of the normals of the faces that meet at that point, counting only
// faces within `creaseAngle` (radians) of the corner's own face. That keeps
// sharp edges sharp: with a 60 degree crease angle a cube's 90 degree edges
// stay crisp, while the gentle angles between a torus's faces blend smoothly.
void computeCornerNormals(Mesh& mesh, float creaseAngle);
