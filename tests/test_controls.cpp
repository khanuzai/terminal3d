#include "controls.h"
#include "test.h"

namespace {

InputEvent key(char c) { return {InputEvent::Type::Char, c}; }

InputEvent mouse(InputEvent::Type type, int x, int y) {
    InputEvent e{type};
    e.x = x;
    e.y = y;
    return e;
}

} // namespace

TEST(controls_quit_keys) {
    ViewState view;
    CHECK(applyInput(view, key('q')));
    CHECK(applyInput(view, InputEvent{InputEvent::Type::Escape}));
    CHECK(!applyInput(view, key('x')));
}

TEST(controls_zoom_and_pitch_stay_in_range) {
    ViewState view;
    for (int i = 0; i < 100; ++i) applyInput(view, key('+'));
    CHECK_NEAR(view.zoom, 8.0, 1e-5);
    for (int i = 0; i < 100; ++i) applyInput(view, InputEvent{InputEvent::Type::Up});
    CHECK_NEAR(view.pitch, -1.5, 1e-5); // never tips over the top
}

TEST(controls_reset_keeps_display_settings) {
    ViewState view;
    applyInput(view, key('w'));
    applyInput(view, key('+'));
    applyInput(view, InputEvent{InputEvent::Type::Left});
    applyInput(view, key('r'));
    CHECK_NEAR(view.zoom, 1.0, 1e-6);
    CHECK_NEAR(view.yaw, 0.0, 1e-6);
    CHECK(view.wireframe); // still on
}

TEST(controls_drag_rotates_and_pauses_the_spin) {
    ViewState view;
    const float pitch = view.pitch;
    applyInput(view, mouse(InputEvent::Type::MousePress, 10, 10));
    applyInput(view, mouse(InputEvent::Type::MouseDrag, 20, 11));
    CHECK(view.yaw > 0.0f);     // dragged right: turns right
    CHECK(view.pitch > pitch);  // dragged down: tips toward us

    // While the button is held, the spin doesn't move the model.
    const float yaw = view.yaw;
    advanceView(view, 0.1f);
    CHECK_EQ(view.yaw, yaw);

    applyInput(view, mouse(InputEvent::Type::MouseRelease, 20, 11));
    advanceView(view, 0.1f);
    CHECK(view.yaw > yaw);
}
