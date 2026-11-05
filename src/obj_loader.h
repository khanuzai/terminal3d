#pragma once

#include <string>

#include "mesh.h"

// Loads a Wavefront .obj file into a triangle mesh.
//
// Supported: "v x y z" vertex lines and "f ..." face lines, where each face
// corner may be written as "v", "v/vt", "v//vn" or "v/vt/vn". Faces with more
// than three corners are split into triangles. Negative (relative) indices are
// supported. All other line types (vt, vn, o, g, usemtl, ...) are ignored.
//
// Throws std::runtime_error if the file can't be opened or is malformed.
Mesh loadObj(const std::string& path);
