#pragma once

#include "input.h"

// Everything the user can change about how the model is shown.
struct ViewState {
    float yaw = 0.0f;       // turn around the vertical axis, in radians
    float pitch = 0.4f;     // tilt toward the camera, in radians
    float zoom = 1.0f;      // > 1 magnifies, < 1 shrinks
    bool autoSpin = true;   // keep turning on its own
    float spinSpeed = 0.8f; // radians per second; negative spins the other way
    bool wireframe = false; // draw edges instead of shaded triangles

    // A mouse drag in progress, and the cell the mouse was last seen at.
    bool dragging = false;
    int dragX = 0;
    int dragY = 0;
};

// Applies one input event to the view. Returns true if the event asks to quit.
//
//   drag        rotate          scroll  zoom in / out
//   arrow keys  rotate          + / -   zoom in / out
//   space       pause the spin  w       toggle wireframe
//   r           reset the view  q, Esc  quit
bool applyInput(ViewState& view, const InputEvent& event);

// Moves time-based animation (the auto-spin) forward by dt seconds.
void advanceView(ViewState& view, float dt);
