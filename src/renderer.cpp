#include "renderer.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

// Where a vertex lands on the screen, and whether it can be drawn at all.
struct ScreenPoint {
    int x = 0;
    int y = 0;
    bool visible = false;
};

// Runs one vertex through the whole pipeline:
//   model space --(mvp)--> clip space --(divide by w)--> normalized device
//   coordinates, where the visible area is -1..1 on x and y --> cells.
ScreenPoint projectToScreen(const Vec3& p, const Mat4& mvp, int width, int height) {
    const Vec4 clip = mvp * Vec4(p, 1.0f);

    // w is the distance in front of the camera. A point at or behind the
    // camera has no sensible position on screen, so we don't draw it.
    if (clip.w <= 1e-5f) return {};

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;

    // Map -1..1 to 0..width and 0..height. Screen y grows downward but NDC y
    // grows upward, hence the (1 - ndcY).
    ScreenPoint s;
    s.x = static_cast<int>(std::floor((ndcX + 1.0f) * 0.5f * width));
    s.y = static_cast<int>(std::floor((1.0f - ndcY) * 0.5f * height));
    s.visible = true;
    return s;
}

// Picks the ASCII character that best matches a line's direction on screen.
// Vertical distances are scaled by kCellAspect so that angles are judged as
// they actually look, not in raw cell counts.
char lineChar(int dx, int dy) {
    const float ax = static_cast<float>(std::abs(dx));
    const float ay = static_cast<float>(std::abs(dy)) * kCellAspect;
    if (ay < ax * 0.5f) return '-';
    if (ax < ay * 0.5f) return '|';
    // Screen y points down, so "right and down" is a backslash.
    return (dx > 0) == (dy > 0) ? '\\' : '/';
}

} // namespace

void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1) {
    const char c = lineChar(x1 - x0, y1 - y0);

    // Bresenham's algorithm: step one cell at a time along the line, using
    // only integer math. `err` tracks how far the true line is from the cells
    // we've chosen, and tells us when to also step in the other direction.
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        fb.set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawWireframe(Framebuffer& fb, const Mesh& mesh, const Mat4& mvp) {
    // Project each vertex once. A vertex is shared by several triangles, so
    // this is much cheaper than projecting the corners of every triangle.
    std::vector<ScreenPoint> points;
    points.reserve(mesh.vertices.size());
    for (const Vec3& v : mesh.vertices) {
        points.push_back(projectToScreen(v, mvp, fb.width(), fb.height()));
    }

    auto edge = [&](int i, int j) {
        const ScreenPoint& a = points[i];
        const ScreenPoint& b = points[j];
        if (a.visible && b.visible) drawLine(fb, a.x, a.y, b.x, b.y);
    };

    // Edges shared by two triangles get drawn twice. That's harmless here;
    // step 5 replaces the wireframe with filled triangles anyway.
    for (const Triangle& t : mesh.triangles) {
        edge(t.a, t.b);
        edge(t.b, t.c);
        edge(t.c, t.a);
    }
}
