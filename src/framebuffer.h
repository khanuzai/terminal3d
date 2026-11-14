#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A 24-bit RGB color, 0-255 per channel.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

// How pixels are turned into terminal characters.
enum class OutputMode {
    // Two pixels per cell, one above the other, drawn as '▀': the character's
    // color paints the top pixel and the cell's background paints the bottom.
    // Terminal cells are about twice as tall as wide, so the pixels are square.
    HalfBlock,
    // One pixel per cell, drawn as a character from a brightness ramp
    // (".:-=+*#%@") in the pixel's color. Pixels are twice as tall as wide.
    Ascii,
};

// An off-screen image that the renderer draws into. We draw a whole frame
// here first, then turn it into one string and send it to the terminal at once.
//
// It has three layers:
//   - pixels: a color per pixel, plus whether anything was drawn there at all
//     (pixels nothing was drawn on show the terminal's own background)
//   - depth: the z-buffer, the depth of the closest surface at each pixel
//   - text: characters drawn on top of everything, in terminal cells
class Framebuffer {
public:
    // cols and rows are the terminal size in cells. The pixel size depends
    // on the output mode. lightBackground reverses the ASCII brightness ramp,
    // because on a light background, more ink looks darker, not brighter.
    Framebuffer(int cols, int rows, OutputMode mode, bool lightBackground = false);

    // Size in pixels: what the 3D renderer draws into.
    int width() const { return width_; }
    int height() const { return height_; }

    // Size in terminal cells: what text is positioned in.
    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // How tall one pixel looks compared to its width on screen. The camera
    // uses this to keep shapes from looking stretched.
    float pixelAspect() const { return mode_ == OutputMode::HalfBlock ? 1.0f : 2.0f; }

    // Erases all pixels and text and resets the depth buffer to "infinitely far".
    void clear();

    // Sets one pixel, ignoring depth. Used for 2D drawing like lines.
    // Coordinates outside the image are ignored ("clipped"), so drawing code
    // never has to check bounds itself.
    void setPixel(int x, int y, const Color& c);

    // Sets one pixel only if `depth` is closer (smaller) than what's already
    // there, and records the new depth. Clipped like setPixel().
    void setPixelIfCloser(int x, int y, float depth, const Color& c);

    // Writes text left to right starting at cell (col, row), on top of the
    // image, in the terminal's default colors. Clipped to the screen.
    void drawText(int col, int row, const std::string& text);

    // Converts the frame into characters, color escape codes and cursor
    // movements that draw it when written to the terminal.
    std::string toAnsi() const;

private:
    std::size_t pixelIndex(int x, int y) const {
        return static_cast<std::size_t>(y) * width_ + x;
    }

    OutputMode mode_;
    bool lightBackground_;
    int cols_;
    int rows_;
    int width_;
    int height_;

    // All pixel layers are stored row by row: pixel (x, y) is at y * width_ + x.
    std::vector<Color> colors_;
    std::vector<std::uint8_t> covered_; // 1 if something was drawn on the pixel
    std::vector<float> depth_;

    std::vector<char> text_; // per cell, row by row; '\0' means no text
};
