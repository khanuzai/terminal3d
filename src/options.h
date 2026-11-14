#pragma once

#include <string>

#include "framebuffer.h"

// Everything that can be set from the command line, with its default.
struct Options {
    std::string modelPath;
    OutputMode mode = OutputMode::HalfBlock;
    bool lightBackground = false; // reverse the ASCII brightness ramp
    Color color{255, 150, 60};    // a warm orange
    int fps = 30;
    float spinSpeed = 0.8f;       // radians per second; negative spins the other way
    bool wireframe = false;
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
