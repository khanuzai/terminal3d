#include <exception>
#include <iostream>
#include <string>

#include "framebuffer.h"
#include "obj_loader.h"
#include "terminal.h"

namespace {

// Draws the outline of a w x h rectangle whose top-left corner is (x0, y0).
void drawBox(Framebuffer& fb, int x0, int y0, int w, int h) {
    if (w < 2 || h < 2) return;
    const int x1 = x0 + w - 1;
    const int y1 = y0 + h - 1;

    for (int x = x0 + 1; x < x1; ++x) {
        fb.set(x, y0, '-');
        fb.set(x, y1, '-');
    }
    for (int y = y0 + 1; y < y1; ++y) {
        fb.set(x0, y, '|');
        fb.set(x1, y, '|');
    }
    fb.set(x0, y0, '+');
    fb.set(x1, y0, '+');
    fb.set(x0, y1, '+');
    fb.set(x1, y1, '+');
}

// Draws text horizontally centered on row y.
void drawCentered(Framebuffer& fb, int y, const std::string& text) {
    fb.drawText((fb.width() - static_cast<int>(text.size())) / 2, y, text);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <model.obj>\n";
        return 1;
    }

    Mesh mesh;
    try {
        mesh = loadObj(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Draw a test screen into an off-screen framebuffer the size of the terminal.
    const TermSize size = terminalSize();
    Framebuffer fb(size.cols, size.rows);
    fb.clear();
    drawBox(fb, 0, 0, fb.width(), fb.height());

    const int mid = fb.height() / 2;
    drawCentered(fb, mid - 6, "term3d");
    drawCentered(fb, mid - 5, std::string(argv[1]) + ": " +
                                  std::to_string(mesh.vertices.size()) + " vertices, " +
                                  std::to_string(mesh.triangles.size()) + " triangles");
    drawCentered(fb, mid - 4, "terminal: " + std::to_string(size.cols) + " x " +
                                  std::to_string(size.rows) + " cells");

    // Terminal cells are about twice as tall as they are wide, so a box that is
    // 8x8 *cells* looks tall and thin, while 16x8 cells looks roughly square.
    // The 3D renderer will have to correct for this.
    const int left = (fb.width() - 28) / 2;
    drawBox(fb, left, mid - 2, 8, 8);
    drawBox(fb, left + 12, mid - 2, 16, 8);
    fb.drawText(left + 1, mid + 6, "8 x 8");
    fb.drawText(left + 16, mid + 6, "16 x 8");

    drawCentered(fb, fb.height() - 2, "press Enter to quit");

    {
        // The session restores the terminal when this block ends,
        // even if something inside it throws.
        TerminalSession session;
        writeToTerminal(fb.toAnsi());
        std::cin.get();
    }

    return 0;
}
