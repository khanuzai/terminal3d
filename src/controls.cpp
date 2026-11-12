#include "controls.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kRotateStep = 0.15f; // radians per arrow key press
constexpr float kZoomStep = 1.15f;   // zoom factor per + or - press
constexpr float kScrollZoomStep = 1.08f; // per scroll event; trackpads send many of these
constexpr float kDragSpeed = 0.03f;  // radians per cell the mouse moves sideways
constexpr float kCellAspect = 2.0f;  // a cell is about twice as tall as wide
constexpr float kMinZoom = 0.3f;
constexpr float kMaxZoom = 8.0f;
constexpr float kMaxPitch = 1.5f;    // just under 90 degrees, so the model never flips over
constexpr float kSpinSpeed = 0.8f;   // radians per second
constexpr float kTwoPi = 6.28318531f;

} // namespace

bool applyInput(ViewState& view, const InputEvent& event) {
    using Type = InputEvent::Type;

    switch (event.type) {
        // The arrows move the front of the model in the arrow's direction.
        case Type::Left:  view.yaw -= kRotateStep; break;
        case Type::Right: view.yaw += kRotateStep; break;
        case Type::Up:    view.pitch -= kRotateStep; break;
        case Type::Down:  view.pitch += kRotateStep; break;
        case Type::Escape: return true;

        // Dragging turns the model as if you'd grabbed its front and pulled.
        case Type::MousePress:
            view.dragging = true;
            view.dragX = event.x;
            view.dragY = event.y;
            break;
        case Type::MouseDrag:
            if (view.dragging) {
                view.yaw += (event.x - view.dragX) * kDragSpeed;
                // A cell is twice as tall as wide, so one row of movement is
                // as far on screen as two columns: rotate twice as much.
                view.pitch += (event.y - view.dragY) * kDragSpeed * kCellAspect;
                view.dragX = event.x;
                view.dragY = event.y;
            }
            break;
        case Type::MouseRelease: view.dragging = false; break;
        case Type::ScrollUp:     view.zoom *= kScrollZoomStep; break;
        case Type::ScrollDown:   view.zoom /= kScrollZoomStep; break;

        case Type::Char:
            switch (event.ch) {
                case 'q': case 'Q': return true;
                case '+': case '=': view.zoom *= kZoomStep; break; // '=' is + without Shift
                case '-': case '_': view.zoom /= kZoomStep; break;
                case ' ': view.autoSpin = !view.autoSpin; break;
                case 'w': case 'W': view.wireframe = !view.wireframe; break;
                case 'r': case 'R': {
                    // Reset where we're looking from, but keep the display settings.
                    const ViewState defaults;
                    view.yaw = defaults.yaw;
                    view.pitch = defaults.pitch;
                    view.zoom = defaults.zoom;
                    break;
                }
                default: break;
            }
            break;
    }

    view.pitch = std::clamp(view.pitch, -kMaxPitch, kMaxPitch);
    view.zoom = std::clamp(view.zoom, kMinZoom, kMaxZoom);
    return false;
}

void advanceView(ViewState& view, float dt) {
    // Hold still while being dragged, so the model follows the mouse exactly.
    if (view.autoSpin && !view.dragging) {
        // Keep yaw within one turn; floats lose precision as numbers grow.
        view.yaw = std::fmod(view.yaw + kSpinSpeed * dt, kTwoPi);
    }
}
