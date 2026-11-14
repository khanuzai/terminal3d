#include "framebuffer.h"

#include <algorithm>
#include <limits>

namespace {

constexpr float kFarthest = std::numeric_limits<float>::infinity();

// The half-block characters, written out as their UTF-8 bytes.
constexpr const char* kUpperHalf = "\xE2\x96\x80"; // ▀
constexpr const char* kLowerHalf = "\xE2\x96\x84"; // ▄

// Brightness ramp for ASCII mode, from dimmest to brightest: characters with
// more "ink" look brighter on a dark background. No space, so that even a dim
// surface stays visible against the empty background.
constexpr char kShadeRamp[] = ".:-=+*#%@";
constexpr int kShadeLevels = sizeof(kShadeRamp) - 1; // minus the '\0'

char shadeChar(const Color& c, bool lightBackground) {
    // The brightest channel works as "how lit is this pixel" for any hue.
    const int brightest = std::max({c.r, c.g, c.b});
    const int level = brightest * kShadeLevels / 256;
    return kShadeRamp[lightBackground ? kShadeLevels - 1 - level : level];
}

// Appends ANSI color codes to a string, remembering which colors are already
// active so it only emits a code when a color actually changes. Adjacent
// cells usually share colors, so this shrinks frames a lot: every 24-bit
// color code is about 19 bytes.
class AnsiColorState {
public:
    explicit AnsiColorState(std::string& out) : out_(out) {}

    void foreground(const Color& c) {
        if (hasFg_ && fg_ == c) return;
        appendColor("38", c); // ESC[38;2;R;G;Bm sets the text color
        fg_ = c;
        hasFg_ = true;
    }

    void background(const Color& c) {
        if (hasBg_ && bg_ == c) return;
        appendColor("48", c); // ESC[48;2;R;G;Bm sets the background color
        bg_ = c;
        hasBg_ = true;
    }

    // Back to the terminal's own colors.
    void defaultForeground() {
        if (!hasFg_) return;
        out_ += "\x1b[39m";
        hasFg_ = false;
    }

    void defaultBackground() {
        if (!hasBg_) return;
        out_ += "\x1b[49m";
        hasBg_ = false;
    }

private:
    void appendColor(const char* layer, const Color& c) {
        out_ += "\x1b[";
        out_ += layer;
        out_ += ";2;";
        out_ += std::to_string(c.r);
        out_ += ';';
        out_ += std::to_string(c.g);
        out_ += ';';
        out_ += std::to_string(c.b);
        out_ += 'm';
    }

    std::string& out_;
    Color fg_;
    Color bg_;
    bool hasFg_ = false; // false means "the terminal's default color"
    bool hasBg_ = false;
};

} // namespace

Framebuffer::Framebuffer(int cols, int rows, OutputMode mode, bool lightBackground)
    : mode_(mode),
      lightBackground_(lightBackground),
      cols_(std::max(cols, 0)),
      rows_(std::max(rows, 0)),
      width_(cols_),
      height_(mode == OutputMode::HalfBlock ? rows_ * 2 : rows_),
      colors_(static_cast<std::size_t>(width_) * height_),
      covered_(colors_.size(), 0),
      depth_(colors_.size(), kFarthest),
      text_(static_cast<std::size_t>(cols_) * rows_, '\0') {}

void Framebuffer::clear() {
    std::fill(covered_.begin(), covered_.end(), 0);
    std::fill(depth_.begin(), depth_.end(), kFarthest);
    std::fill(text_.begin(), text_.end(), '\0');
}

void Framebuffer::setPixel(int x, int y, const Color& c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    const std::size_t i = pixelIndex(x, y);
    colors_[i] = c;
    covered_[i] = 1;
}

void Framebuffer::setPixelIfCloser(int x, int y, float depth, const Color& c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    const std::size_t i = pixelIndex(x, y);
    if (depth >= depth_[i]) return; // something closer is already drawn here
    depth_[i] = depth;
    colors_[i] = c;
    covered_[i] = 1;
}

void Framebuffer::drawText(int col, int row, const std::string& text) {
    if (row < 0 || row >= rows_) return;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const int c = col + static_cast<int>(i);
        if (c < 0 || c >= cols_) continue;
        text_[static_cast<std::size_t>(row) * cols_ + c] = text[i];
    }
}

std::string Framebuffer::toAnsi() const {
    std::string out;
    // Rough guess so the string rarely has to grow: a few bytes per cell.
    out.reserve(static_cast<std::size_t>(cols_) * rows_ * 8);

    // Start from the default colors, since we don't know what the terminal
    // was left with.
    out += "\x1b[0m";
    AnsiColorState color(out);

    for (int row = 0; row < rows_; ++row) {
        // "ESC [ row ; col H" moves the cursor (rows and columns count from 1).
        // Jumping to the start of each row, instead of printing '\n', means a
        // full-width row can never wrap or scroll the screen.
        out += "\x1b[" + std::to_string(row + 1) + ";1H";

        for (int col = 0; col < cols_; ++col) {
            // Text sits on top of the image, in the terminal's own colors.
            const char ch = text_[static_cast<std::size_t>(row) * cols_ + col];
            if (ch != '\0') {
                color.defaultForeground();
                color.defaultBackground();
                out += ch;
                continue;
            }

            if (mode_ == OutputMode::Ascii) {
                const std::size_t i = pixelIndex(col, row);
                color.defaultBackground();
                if (covered_[i]) {
                    color.foreground(colors_[i]);
                    out += shadeChar(colors_[i], lightBackground_);
                } else {
                    out += ' ';
                }
                continue;
            }

            // Half-block mode: this cell shows pixel rows 2*row and 2*row + 1.
            // Empty pixels let the terminal's background show through.
            const std::size_t top = pixelIndex(col, row * 2);
            const std::size_t bottom = pixelIndex(col, row * 2 + 1);
            if (covered_[top] && covered_[bottom]) {
                color.foreground(colors_[top]);
                color.background(colors_[bottom]);
                out += kUpperHalf;
            } else if (covered_[top]) {
                color.foreground(colors_[top]);
                color.defaultBackground();
                out += kUpperHalf;
            } else if (covered_[bottom]) {
                color.foreground(colors_[bottom]);
                color.defaultBackground();
                out += kLowerHalf;
            } else {
                color.defaultBackground(); // a space only shows the background
                out += ' ';
            }
        }
    }

    // Leave the terminal in its normal colors.
    out += "\x1b[0m";
    return out;
}
