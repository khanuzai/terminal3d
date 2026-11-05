#include <exception>
#include <iostream>

#include "obj_loader.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <model.obj>\n";
        return 1;
    }

    try {
        const Mesh mesh = loadObj(argv[1]);
        std::cout << "Loaded " << argv[1] << "\n"
                  << "  vertices:  " << mesh.vertices.size() << "\n"
                  << "  triangles: " << mesh.triangles.size() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
