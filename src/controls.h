#pragma once

#include "input.h"

// Everything the user can change about how the model is shown.
struct ViewState {
    float yaw = 0.0f;       // turn around the vertical axis, in radians
    float pitch = 0.4f;     // tilt toward the camera, in radians
    float zoom = 1.0f;      // > 1 magnifies, < 1 shrinks
    bool autoSpin = true;   // keep turning on its own
    bool wireframe = false; // draw edges instead of shaded triangles
};

// Applies one input event to the view. Returns true if the event asks to quit.
//
//   arrow keys  rotate          + / -  zoom in / out
//   space       pause the spin  w      toggle wireframe
//   r           reset the view  q, Esc quit
bool applyInput(ViewState& view, const InputEvent& event);

// Moves time-based animation (the auto-spin) forward by dt seconds.
void advanceView(ViewState& view, float dt);
