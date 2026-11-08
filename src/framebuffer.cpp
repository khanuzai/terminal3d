#include "framebuffer.h"

#include <algorithm>
#include <limits>

namespace {

constexpr float kFarthest = std::numeric_limits<float>::infinity();

} // namespace

Framebuffer::Framebuffer(int width, int height)
    : width_(std::max(width, 0)),
      height_(std::max(height, 0)),
      cells_(static_cast<std::size_t>(width_) * height_, ' '),
      depth_(cells_.size(), kFarthest) {}

void Framebuffer::clear(char c) {
    std::fill(cells_.begin(), cells_.end(), c);
    std::fill(depth_.begin(), depth_.end(), kFarthest);
}

void Framebuffer::set(int x, int y, char c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    cells_[static_cast<std::size_t>(y) * width_ + x] = c;
}

void Framebuffer::setIfCloser(int x, int y, float depth, char c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    const std::size_t i = static_cast<std::size_t>(y) * width_ + x;
    if (depth >= depth_[i]) return; // something closer is already drawn here
    depth_[i] = depth;
    cells_[i] = c;
}

void Framebuffer::drawText(int x, int y, const std::string& text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        set(x + static_cast<int>(i), y, text[i]);
    }
}

std::string Framebuffer::toAnsi() const {
    std::string out;
    out.reserve(cells_.size() + static_cast<std::size_t>(height_) * 10);

    for (int y = 0; y < height_; ++y) {
        // "ESC [ row ; col H" moves the cursor (rows and columns count from 1).
        // Jumping to the start of each row, instead of printing '\n', means a
        // full-width row can never wrap or scroll the screen.
        out += "\x1b[" + std::to_string(y + 1) + ";1H";
        out.append(cells_.data() + static_cast<std::size_t>(y) * width_, width_);
    }
    return out;
}
