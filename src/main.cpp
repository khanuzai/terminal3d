#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "framebuffer.h"
#include "obj_loader.h"
#include "renderer.h"
#include "terminal.h"

namespace {

constexpr int kTargetFps = 30;
constexpr float kSpinSpeed = 0.8f; // radians per second around the vertical axis
constexpr float kTilt = 0.4f;      // radians; tips the model so we see its top

// Direction toward the light in world space: up, to the left, and in front of
// the model (the camera is on the +z side). It stays fixed while the model
// spins, so you can watch each face brighten and dim as it turns.
constexpr Vec3 kToLight{-0.5f, 0.8f, 1.0f};

// Everything that depends on the terminal's size. Rebuilt when the window is resized.
struct Viewport {
    Framebuffer fb;
    Mat4 viewProjection;
};

Viewport makeViewport(const TermSize& size) {
    // The screen is `cols` cells wide and `rows` cells tall, and each cell is
    // kCellAspect times taller than wide. So in real, visible proportions the
    // screen is cols wide and rows * kCellAspect tall.
    const float pi = 3.14159265f;
    const float fovY = 60.0f * pi / 180.0f;
    const float aspect = static_cast<float>(size.cols) / (size.rows * kCellAspect);

    // Pull the camera back far enough that the unit sphere fits in view. On a
    // narrow (portrait) window the horizontal field of view is the tighter one.
    const float halfFov = std::atan(std::tan(fovY / 2.0f) * std::min(1.0f, aspect));
    const float distance = 1.1f / std::sin(halfFov); // 1.1 leaves a little margin

    // view: move the world away from the camera, which sits at the origin.
    // projection: perspective, from 3D to the 2D screen.
    const Mat4 view = Mat4::translation({0.0f, 0.0f, -distance});
    const Mat4 projection = Mat4::perspective(fovY, aspect, 0.1f, 100.0f);
    return {Framebuffer(size.cols, size.rows), projection * view};
}

} // namespace

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

    const Mat4 fit = fitToUnitSphere(mesh);
    const std::string info = std::string(argv[1]) + "  " +
                             std::to_string(mesh.vertices.size()) + " vertices, " +
                             std::to_string(mesh.triangles.size()) + " triangles";

    // Install the handlers before touching the terminal, so a Ctrl+C at any
    // point after this ends the loop normally and the session gets restored.
    installSignalHandlers();
    TerminalSession session;
    Viewport vp = makeViewport(terminalSize());

    using Clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / kTargetFps));
    const auto start = Clock::now();
    auto nextFrame = start;

    // FPS counter: count frames over each one-second window, then show that count.
    auto fpsWindowStart = start;
    int framesInWindow = 0;
    int fps = 0; // 0 until the first full second has passed

    while (!quitRequested()) {
        if (consumeResize()) {
            vp = makeViewport(terminalSize());
            clearTerminal();
        }

        // Animate from real elapsed time, not from a frame counter, so the
        // model spins at the same speed however fast or slow frames are.
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - start).count();

        const float windowSeconds = std::chrono::duration<float>(now - fpsWindowStart).count();
        if (windowSeconds >= 1.0f) {
            fps = static_cast<int>(std::lround(framesInWindow / windowSeconds));
            framesInWindow = 0;
            fpsWindowStart = now;
        }
        ++framesInWindow; // this frame belongs to the window that starts now

        const Mat4 model = Mat4::rotationX(kTilt) * Mat4::rotationY(elapsed * kSpinSpeed) * fit;

        vp.fb.clear();
        const int drawn = drawShaded(vp.fb, mesh, model, vp.viewProjection, kToLight);
        vp.fb.drawText(1, 0, info + ", " + std::to_string(drawn) + " drawn");
        vp.fb.drawText(1, vp.fb.height() - 1,
                       (fps > 0 ? std::to_string(fps) : std::string("--")) +
                           " fps   Ctrl+C to quit");
        presentFrame(vp.fb.toAnsi());

        // Sleep until a fixed schedule of frame times, not "33 ms from now",
        // so the time spent drawing doesn't add up and slow the frame rate.
        // If we've fallen behind, start the schedule again instead of rushing
        // through several frames to catch up.
        nextFrame += frameDuration;
        if (nextFrame < Clock::now()) nextFrame = Clock::now();
        std::this_thread::sleep_until(nextFrame);
    }

    // `session` is destroyed here, which shows the cursor and leaves the
    // alternate screen.
    return 0;
}
