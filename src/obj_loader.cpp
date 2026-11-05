#include "obj_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// Builds an error that points at the offending line, like "cube.obj:12: ...".
std::runtime_error parseError(const std::string& path, int lineNo, const std::string& msg) {
    return std::runtime_error(path + ":" + std::to_string(lineNo) + ": " + msg);
}

// Turns one face corner token ("7", "7/2", "7//4", "7/2/4") into a 0-based
// vertex index. We only need the position index: the part before the first '/'.
int parseFaceIndex(const std::string& token, int vertexCount,
                   const std::string& path, int lineNo) {
    const std::string posPart = token.substr(0, token.find('/'));

    int index = 0;
    try {
        std::size_t consumed = 0;
        index = std::stoi(posPart, &consumed);
        if (consumed != posPart.size()) throw std::invalid_argument("trailing characters");
    } catch (const std::exception&) {
        throw parseError(path, lineNo, "invalid face index '" + token + "'");
    }

    // OBJ indices start at 1. Negative indices count back from the most
    // recently defined vertex (-1 = last one), and 0 is never valid.
    const int resolved = index > 0 ? index - 1 : vertexCount + index;
    if (index == 0 || resolved < 0 || resolved >= vertexCount) {
        throw parseError(path, lineNo, "face index '" + token + "' is out of range");
    }
    return resolved;
}

} // namespace

Mesh loadObj(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("could not open '" + path + "'");

    Mesh mesh;
    std::vector<int> face; // corner indices of the face being parsed, reused per line
    std::string line;
    int lineNo = 0;

    while (std::getline(file, line)) {
        ++lineNo;

        // Drop comments, which run from '#' to the end of the line.
        line = line.substr(0, line.find('#'));

        std::istringstream in(line);
        std::string keyword;
        if (!(in >> keyword)) continue; // blank line

        if (keyword == "v") {
            Vec3 v;
            if (!(in >> v.x >> v.y >> v.z)) {
                throw parseError(path, lineNo, "expected 3 numbers after 'v'");
            }
            mesh.vertices.push_back(v);
        } else if (keyword == "f") {
            face.clear();
            const int vertexCount = static_cast<int>(mesh.vertices.size());
            std::string token;
            while (in >> token) {
                face.push_back(parseFaceIndex(token, vertexCount, path, lineNo));
            }
            if (face.size() < 3) {
                throw parseError(path, lineNo, "a face needs at least 3 vertices");
            }

            // Fan triangulation: a polygon (0, 1, 2, 3, ...) becomes the
            // triangles (0,1,2), (0,2,3), ... This works for convex polygons,
            // which is what nearly all exported models contain.
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                mesh.triangles.push_back({face[0], face[i], face[i + 1]});
            }
        }
        // Every other keyword is ignored for now.
    }

    return mesh;
}
