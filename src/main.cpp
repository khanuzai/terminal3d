#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>

#include "framebuffer.h"
#include "obj_loader.h"
#include "renderer.h"
#include "terminal.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <model.obj>\n";
        return 1;
    }

    Mesh mesh;
    try {
        mesh = loadObj(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    const TermSize size = terminalSize();
    Framebuffer fb(size.cols, size.rows);

    // Camera setup. The screen is `cols` cells wide, but only `rows` cells
    // tall, and each cell is kCellAspect times taller than wide. So in real,
    // visible proportions the screen is cols wide and rows * kCellAspect tall.
    const float pi = 3.14159265f;
    const float fovY = 60.0f * pi / 180.0f;
    const float aspect = static_cast<float>(size.cols) / (size.rows * kCellAspect);

    // Pull the camera back far enough that the unit sphere fits in view. On a
    // narrow (portrait) window the horizontal field of view is the tighter one.
    const float halfFov = std::atan(std::tan(fovY / 2.0f) * std::min(1.0f, aspect));
    const float distance = 1.1f / std::sin(halfFov); // 1.1 leaves a little margin

    // model: center and scale the mesh, then turn it so we see three sides.
    // view: move the world away from the camera, which sits at the origin.
    // projection: perspective, from 3D to the 2D screen.
    const Mat4 model = Mat4::rotationX(0.5f) * Mat4::rotationY(0.7f) * fitToUnitSphere(mesh);
    const Mat4 view = Mat4::translation({0.0f, 0.0f, -distance});
    const Mat4 projection = Mat4::perspective(fovY, aspect, 0.1f, 100.0f);
    const Mat4 mvp = projection * view * model;

    fb.clear();
    drawWireframe(fb, mesh, mvp);
    fb.drawText(1, 0, std::string(argv[1]) + "  " + std::to_string(mesh.vertices.size()) +
                          " vertices, " + std::to_string(mesh.triangles.size()) + " triangles");
    fb.drawText(1, fb.height() - 1, "press Enter to quit");

    {
        // The session restores the terminal when this block ends,
        // even if something inside it throws.
        TerminalSession session;
        writeToTerminal(fb.toAnsi());
        std::cin.get();
    }

    return 0;
}
