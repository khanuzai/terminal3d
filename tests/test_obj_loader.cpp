#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "obj_loader.h"
#include "test.h"

namespace {

// Writes `contents` to a file in the system's temporary directory, and deletes
// it again when the TempObj goes out of scope. Keeping each test's .obj text
// inline, right next to the checks, makes the tests easy to read.
struct TempObj {
    std::string path;

    explicit TempObj(const std::string& contents) {
        static int counter = 0;
        path = (std::filesystem::temp_directory_path() /
                ("term3d_test_" + std::to_string(++counter) + ".obj"))
                   .string();
        std::ofstream(path) << contents;
    }
    ~TempObj() { std::remove(path.c_str()); }

    TempObj(const TempObj&) = delete;
    TempObj& operator=(const TempObj&) = delete;
};

} // namespace

TEST(obj_reads_vertices_and_all_corner_formats) {
    const TempObj file(
        "# a comment line\n"
        "v 0 0 0\n"
        "v 1 0 0   # a comment after data\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vn 0 0 1\n"
        "\n"
        "f 1 2 3\n"
        "f 1/1 3/1 4/1\n"
        "f 1//1 2//1 3//1\n"
        "f 1/1/1 3/1/1 4/1/1\n");
    const Mesh mesh = loadObj(file.path);

    REQUIRE(mesh.vertices.size() == 4u);
    CHECK_NEAR(mesh.vertices[2].x, 1.0, 1e-6);
    CHECK_NEAR(mesh.vertices[2].y, 1.0, 1e-6);

    REQUIRE(mesh.triangles.size() == 4u);
    // Indices become 0-based.
    CHECK_EQ(mesh.triangles[1].a, 0);
    CHECK_EQ(mesh.triangles[1].b, 2);
    CHECK_EQ(mesh.triangles[1].c, 3);
    CHECK_EQ(mesh.triangles[3].c, 3);
}

TEST(obj_splits_polygons_into_a_fan_of_triangles) {
    const TempObj file(
        "v 0 0 0\nv 1 0 0\nv 2 1 0\nv 1 2 0\nv 0 1 0\n"
        "f 1 2 3 4 5\n");
    const Mesh mesh = loadObj(file.path);

    // A pentagon becomes 3 triangles, all sharing the first corner.
    REQUIRE(mesh.triangles.size() == 3u);
    for (const Triangle& t : mesh.triangles) CHECK_EQ(t.a, 0);
    CHECK_EQ(mesh.triangles[0].b, 1);
    CHECK_EQ(mesh.triangles[2].c, 4);
}

TEST(obj_negative_indices_count_back_from_the_latest_vertex) {
    const TempObj file(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f -3 -2 -1\n"
        "v 5 5 5\n"
        "f -4 -3 -1\n"); // -1 is now the 4th vertex
    const Mesh mesh = loadObj(file.path);

    REQUIRE(mesh.triangles.size() == 2u);
    CHECK_EQ(mesh.triangles[0].a, 0);
    CHECK_EQ(mesh.triangles[0].c, 2);
    CHECK_EQ(mesh.triangles[1].a, 0);
    CHECK_EQ(mesh.triangles[1].c, 3);
}

TEST(obj_errors_name_the_file_and_line) {
    const TempObj outOfRange("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
    CHECK_THROWS(std::runtime_error, loadObj(outOfRange.path), ":4: face index '4' is out of range");

    const TempObj zero("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n");
    CHECK_THROWS(std::runtime_error, loadObj(zero.path), "out of range");

    const TempObj garbage("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2x 3\n");
    CHECK_THROWS(std::runtime_error, loadObj(garbage.path), ":4: invalid face index '2x'");

    const TempObj tooFew("v 0 0 0\nv 1 0 0\nf 1 2\n");
    CHECK_THROWS(std::runtime_error, loadObj(tooFew.path), ":3: a face needs at least 3 vertices");

    const TempObj badVertex("v 0 0\n");
    CHECK_THROWS(std::runtime_error, loadObj(badVertex.path), ":1: expected 3 numbers after 'v'");
}

TEST(obj_missing_file_is_an_error) {
    CHECK_THROWS(std::runtime_error, loadObj("/no/such/dir/model.obj"), "could not open");
}
