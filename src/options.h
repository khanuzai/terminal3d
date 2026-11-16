#pragma once

#include <string>

#include "framebuffer.h"

// Whether to draw the back faces of triangles.
enum class FaceMode {
    Auto,     // two-sided for open meshes, one-sided (faster) for closed ones
    OneSided,
    TwoSided,
};

// Everything that can be set from the command line, with its default.
struct Options {
    std::string modelPath;
    OutputMode mode = OutputMode::HalfBlock;
    bool lightBackground = false; // reverse the ASCII brightness ramp
    Color color{255, 150, 60};    // a warm orange
    int fps = 30;
    float spinSpeed = 0.8f;       // radians per second; negative spins the other way
    bool wireframe = false;
    FaceMode faces = FaceMode::Auto;
    bool flip = false;            // reverse every triangle's winding after loading
    bool smooth = true;           // smooth (Gouraud) shading; false = flat
    float creaseAngle = 60.0f;    // degrees; sharper edges than this stay sharp
    bool showHelp = false;
};

// Parses the command line. Options may come before or after the model path,
// and values can be given as "--fps 60" or "--fps=60". "--" ends the options,
// so a file whose name starts with '-' can still be loaded.
//
// Throws std::invalid_argument with a message meant for the user if the
// arguments are wrong. With --help, the model path may be left out.
Options parseOptions(int argc, const char* const* argv);

// The text printed by --help.
std::string usageText();
