#pragma once

#include <string>
#include <vector>

// A 2D grid of character cells, with (0, 0) at the top-left corner.
// We draw a whole frame into this off-screen grid first, then turn it into
// one string and send it to the terminal all at once.
//
// Alongside the characters it keeps a depth buffer (z-buffer): for every cell,
// how far away the surface drawn there is. That lets 3D drawing keep only the
// closest surface at each cell, whatever order triangles are drawn in.
class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    // Fills every cell with c and resets the depth buffer to "infinitely far".
    void clear(char c = ' ');

    // Sets one cell, ignoring depth. Used for text and 2D drawing.
    // Coordinates outside the grid are ignored ("clipped"), so drawing code
    // never has to check bounds itself.
    void set(int x, int y, char c);

    // Sets one cell only if `depth` is closer (smaller) than what's already
    // there, and records the new depth. Clipped like set().
    void setIfCloser(int x, int y, float depth, char c);

    // Writes text left to right starting at (x, y). Clipped like set().
    void drawText(int x, int y, const std::string& text);

    // Converts the grid into a string of characters and cursor-movement
    // escape codes that draws the whole frame when written to the terminal.
    std::string toAnsi() const;

private:
    int width_;
    int height_;
    std::vector<char> cells_;   // row by row: cell (x, y) is at y * width_ + x
    std::vector<float> depth_;  // same layout as cells_
};
