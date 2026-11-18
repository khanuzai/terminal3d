#include <algorithm>
#include <cmath>
#include <string>

#include "mesh.h"
#include "obj_loader.h"
#include "test.h"

namespace {

constexpr float kPi = 3.14159265f;

// A cube from (0,0,0) to (1,1,1), wound counter-clockwise seen from outside.
// Vertex i is at (i & 1, (i >> 1) & 1, (i >> 2) & 1), so e.g. 7 is (1,1,1).
// Each face is a quad (a, b, c, d) split into (a, b, c) and (a, c, d).
Mesh unitCube() {
    Mesh mesh;
    for (int i = 0; i < 8; ++i) {
        mesh.vertices.push_back({float(i & 1), float((i >> 1) & 1), float((i >> 2) & 1)});
    }
    const int quads[6][4] = {
        {0, 2, 3, 1}, {4, 5, 7, 6}, // -z, +z
        {0, 1, 5, 4}, {2, 6, 7, 3}, // -y, +y
        {0, 4, 6, 2}, {1, 3, 7, 5}, // -x, +x
    };
    for (const auto& q : quads) {
        mesh.triangles.push_back({q[0], q[1], q[2]});
        mesh.triangles.push_back({q[0], q[2], q[3]});
    }
    return mesh;
}

bool sameDirection(const Vec3& a, const Vec3& b, double tolerance = 1e-5) {
    return length(a - b) <= tolerance;
}

} // namespace

TEST(mesh_cube_is_closed_until_a_triangle_is_removed) {
    Mesh cube = unitCube();
    CHECK(isClosed(cube));
    cube.triangles.pop_back(); // now there's a hole
    CHECK(!isClosed(cube));
    CHECK(!isClosed(Mesh{})); // nothing at all isn't a closed surface
}

TEST(mesh_signed_volume_measures_and_detects_inside_out) {
    Mesh cube = unitCube();
    CHECK_NEAR(signedVolume(cube), 1.0, 1e-6);

    // Moving the mesh doesn't change its volume, even though the origin the
    // volume is measured from is now outside it.
    for (Vec3& v : cube.vertices) v += Vec3{3, -7, 2};
    CHECK_NEAR(signedVolume(cube), 1.0, 1e-4);

    flipWinding(cube);
    CHECK_NEAR(signedVolume(cube), -1.0, 1e-4);
}

TEST(mesh_fit_centers_and_scales_into_unit_sphere) {
    Mesh cube = unitCube();
    for (Vec3& v : cube.vertices) v = v * 10.0f + Vec3{100, 0, -50};

    const Mat4 fit = fitToUnitSphere(cube);
    float farthest = 0;
    Vec3 sum;
    for (const Vec3& v : cube.vertices) {
        const Vec3 p = fit.transformPoint(v);
        farthest = std::max(farthest, length(p));
        sum += p;
    }
    CHECK_NEAR(farthest, 1.0, 1e-5);
    CHECK(length(sum) < 1e-4f); // centered: the corners balance out
}

TEST(mesh_crease_angle_keeps_cube_edges_sharp) {
    Mesh cube = unitCube();
    computeCornerNormals(cube, 60.0f * kPi / 180.0f);
    REQUIRE(cube.cornerNormals.size() == cube.triangles.size() * 3);

    // The faces meet at 90 degrees, more than 60, so nothing is averaged:
    // every corner keeps its own face's normal.
    for (std::size_t f = 0; f < cube.triangles.size(); ++f) {
        const Triangle& t = cube.triangles[f];
        const Vec3 faceNormal = normalize(cross(cube.vertices[t.b] - cube.vertices[t.a],
                                                cube.vertices[t.c] - cube.vertices[t.a]));
        for (int k = 0; k < 3; ++k) {
            CHECK(sameDirection(cube.cornerNormals[3 * f + k], faceNormal));
        }
    }
}

TEST(mesh_smoothing_does_not_depend_on_triangulation) {
    // With every edge smoothed, each cube corner's normal must point straight
    // out along the diagonal, (1,1,1)/sqrt(3) at (1,1,1). The three faces
    // there are split into triangles differently: some touch the corner with
    // one triangle, some with two. Weighting by the corner angle makes that
    // not matter (weighting by area would tilt the result).
    Mesh cube = unitCube();
    computeCornerNormals(cube, kPi);

    int checked = 0;
    for (std::size_t f = 0; f < cube.triangles.size(); ++f) {
        const int corners[3] = {cube.triangles[f].a, cube.triangles[f].b, cube.triangles[f].c};
        for (int k = 0; k < 3; ++k) {
            const Vec3 p = cube.vertices[corners[k]];
            const Vec3 expected = normalize(p - Vec3{0.5f, 0.5f, 0.5f});
            CHECK(sameDirection(cube.cornerNormals[3 * f + k], expected));
            ++checked;
        }
    }
    CHECK_EQ(checked, 36);
}

TEST(mesh_smoothing_welds_duplicate_vertices) {
    // Two triangles folded 20 degrees along the edge from (0,0,0) to (0,1,0).
    // The second triangle has its own copies of the shared edge's vertices
    // (indices 3 and 4), as at a seam in a real model.
    Mesh mesh;
    const float bend = 20.0f * kPi / 180.0f;
    mesh.vertices = {
        {0, 0, 0}, {0, 1, 0}, {-1, 0, 0},                   // left triangle
        {0, 0, 0}, {0, 1, 0}, {std::cos(bend), 0, std::sin(bend)}, // right, with copies
    };
    mesh.triangles = {{0, 1, 2}, {3, 5, 4}};
    computeCornerNormals(mesh, 60.0f * kPi / 180.0f);

    // Both sides agree on the normal along the shared edge, so there's no
    // visible seam, and it differs from either face's own normal.
    const Vec3 left = mesh.cornerNormals[0];  // triangle 0, vertex 0
    const Vec3 right = mesh.cornerNormals[3]; // triangle 1, vertex 3 (same place)
    CHECK(sameDirection(left, right));
    CHECK(!sameDirection(left, {0, 0, 1}, 1e-2));

    // The corners not on the shared edge only touch one face, and keep it.
    CHECK(sameDirection(mesh.cornerNormals[2], {0, 0, 1}));
}

TEST(mesh_flip_clears_stale_normals) {
    Mesh cube = unitCube();
    computeCornerNormals(cube, kPi);
    flipWinding(cube);
    CHECK(cube.cornerNormals.empty());
}

#ifdef TERM3D_MODELS_DIR
// The models shipped in the repository load, and are closed and right side out.
TEST(mesh_shipped_cube_and_torus_are_closed_and_outward) {
    for (const char* name : {"cube.obj", "torus.obj"}) {
        const Mesh mesh = loadObj(std::string(TERM3D_MODELS_DIR) + "/" + name);
        CHECK(isClosed(mesh));
        CHECK(signedVolume(mesh) > 0.0);
    }
}

TEST(mesh_torus_smooth_normals_match_the_true_surface) {
    // The torus has radius 1 around the z axis and a tube of radius 0.4. At a
    // point p, the true normal points away from the closest point on the
    // tube's center circle. Smoothed normals should be within a degree of it.
    Mesh torus = loadObj(std::string(TERM3D_MODELS_DIR) + "/torus.obj");
    computeCornerNormals(torus, 60.0f * kPi / 180.0f);

    float worstDegrees = 0;
    for (std::size_t f = 0; f < torus.triangles.size(); ++f) {
        const int corners[3] = {torus.triangles[f].a, torus.triangles[f].b, torus.triangles[f].c};
        for (int k = 0; k < 3; ++k) {
            const Vec3 p = torus.vertices[corners[k]];
            const Vec3 center = normalize(Vec3{p.x, p.y, 0});
            const Vec3 truth = normalize(p - center);
            const float c = std::clamp(dot(truth, torus.cornerNormals[3 * f + k]), -1.0f, 1.0f);
            worstDegrees = std::max(worstDegrees, std::acos(c) * 180.0f / kPi);
        }
    }
    CHECK(worstDegrees < 1.0f);
}
#endif
