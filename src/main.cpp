#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "controls.h"
#include "framebuffer.h"
#include "input.h"
#include "obj_loader.h"
#include "options.h"
#include "renderer.h"
#include "terminal.h"

namespace {

constexpr float kFovY = 60.0f * 3.14159265f / 180.0f; // vertical field of view at zoom 1

// Direction toward the light in world space: up, to the left, and in front of
// the model (the camera is on the +z side). It stays fixed while the model
// spins, so you can watch each face brighten and dim as it turns.
constexpr Vec3 kToLight{-0.5f, 0.8f, 1.0f};

// Everything that depends on the terminal's size. Rebuilt when the window is resized.
struct Viewport {
    Framebuffer fb;
    float aspect;   // visible width / height of the image
    float distance; // how far back the camera sits so the whole model fits
};

Viewport makeViewport(const TermSize& size, const Options& opts) {
    Framebuffer fb(size.cols, size.rows, opts.mode, opts.lightBackground);

    // The image is width x height pixels, and each pixel is pixelAspect times
    // taller than wide. So in real, visible proportions the image is `width`
    // wide and height * pixelAspect tall.
    const float aspect = static_cast<float>(fb.width()) / (fb.height() * fb.pixelAspect());

    // Pull the camera back far enough that the unit sphere fits in view. On a
    // narrow (portrait) window the horizontal field of view is the tighter one.
    const float halfFov = std::atan(std::tan(kFovY / 2.0f) * std::min(1.0f, aspect));
    const float distance = 1.1f / std::sin(halfFov); // 1.1 leaves a little margin

    return {std::move(fb), aspect, distance};
}

// The combined camera (view) and perspective (projection) matrix.
Mat4 viewProjection(const Viewport& vp, float zoom) {
    // Zooming narrows the field of view, like a zoom lens. The camera itself
    // doesn't move, so it can never end up inside the model.
    const float fov = 2.0f * std::atan(std::tan(kFovY / 2.0f) / zoom);
    const Mat4 view = Mat4::translation({0.0f, 0.0f, -vp.distance});
    const Mat4 projection = Mat4::perspective(fov, vp.aspect, 0.1f, 100.0f);
    return projection * view;
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    try {
        opts = parseOptions(argc, argv);
    } catch (const std::invalid_argument& e) {
        // Exit code 2 is the convention for "you called me wrong".
        std::cerr << "term3d: " << e.what() << "\n"
                  << "Try 'term3d --help' for more information.\n";
        return 2;
    }
    if (opts.showHelp) {
        std::cout << usageText();
        return 0;
    }

    Mesh mesh;
    try {
        mesh = loadObj(opts.modelPath);
    } catch (const std::exception& e) {
        std::cerr << "term3d: " << e.what() << "\n";
        return 1;
    }

    const Mat4 fit = fitToUnitSphere(mesh);
    const std::string info = opts.modelPath + "  " +
                             std::to_string(mesh.vertices.size()) + " vertices, " +
                             std::to_string(mesh.triangles.size()) + " triangles";
    const std::string help =
        "drag/arrows rotate  scroll/+- zoom  space spin  w wireframe  r reset  q quit";

    // Install the handlers before touching the terminal, so a Ctrl+C at any
    // point after this ends the loop normally and the session gets restored.
    installSignalHandlers();
    TerminalSession session;
    Viewport vp = makeViewport(terminalSize(), opts);
    InputReader input;

    ViewState view;
    view.spinSpeed = opts.spinSpeed;
    view.wireframe = opts.wireframe;

    using Clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / opts.fps));
    const auto start = Clock::now();
    auto nextFrame = start;
    auto lastFrame = start;

    // FPS counter: count frames over each one-second window, then show that count.
    auto fpsWindowStart = start;
    int framesInWindow = 0;
    int fps = 0; // 0 until the first full second has passed

    bool quit = false;
    while (!quit && !quitRequested()) {
        // Ctrl+Z: hand the terminal back to the shell, then actually stop.
        // stopProcess() returns once we're resumed (e.g. with `fg`), and the
        // SIGCONT that resumed us is handled just below.
        if (consumeSuspend()) {
            session.leave();
            stopProcess();
        }

        // We were resumed. Take the terminal over again. The window may have
        // been resized while we were stopped, so start from its current size.
        if (consumeContinue()) {
            session.enter();
            vp = makeViewport(terminalSize(), opts);
            fpsWindowStart = Clock::now();
            framesInWindow = 0;
        }

        if (consumeResize()) {
            vp = makeViewport(terminalSize(), opts);
            clearTerminal();
        }

        for (const InputEvent& event : input.readEvents()) {
            if (applyInput(view, event)) quit = true;
        }
        if (quit) break;

        // Animate by the real time since the last frame, so the model spins at
        // the same speed however fast or slow frames are. Capping it stops a
        // long pause (a slow frame, or being suspended) from making the model jump.
        const auto now = Clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - lastFrame).count(), 0.1f);
        lastFrame = now;
        advanceView(view, dt);

        const float windowSeconds = std::chrono::duration<float>(now - fpsWindowStart).count();
        if (windowSeconds >= 1.0f) {
            fps = static_cast<int>(std::lround(framesInWindow / windowSeconds));
            framesInWindow = 0;
            fpsWindowStart = now;
        }
        ++framesInWindow; // this frame belongs to the window that starts now

        const Mat4 model = Mat4::rotationX(view.pitch) * Mat4::rotationY(view.yaw) * fit;
        const Mat4 vpMatrix = viewProjection(vp, view.zoom);

        vp.fb.clear();
        std::string status = info;
        if (view.wireframe) {
            drawWireframe(vp.fb, mesh, vpMatrix * model, opts.color);
            status += ", wireframe";
        } else {
            const int drawn = drawShaded(vp.fb, mesh, model, vpMatrix, kToLight, opts.color);
            status += ", " + std::to_string(drawn) + " drawn";
        }
        vp.fb.drawText(1, 0, status);
        vp.fb.drawText(1, vp.fb.rows() - 1,
                       (fps > 0 ? std::to_string(fps) : std::string("--")) + " fps   " + help);

        // If the frame can't be written, the terminal is gone (for example its
        // window was closed): there's nobody left to draw for, so stop.
        if (!presentFrame(vp.fb.toAnsi())) break;

        // Sleep until a fixed schedule of frame times, not "33 ms from now",
        // so the time spent drawing doesn't add up and slow the frame rate.
        // If we've fallen behind, start the schedule again instead of rushing
        // through several frames to catch up.
        nextFrame += frameDuration;
        if (nextFrame < Clock::now()) nextFrame = Clock::now();
        std::this_thread::sleep_until(nextFrame);
    }

    // `session` is destroyed here, which restores the terminal.
    return 0;
}
