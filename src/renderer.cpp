#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

// Where a vertex lands on the screen, and whether it can be drawn at all.
// x and y are in cells but kept as floats, so triangle edges can fall between
// cell centers. z is the depth: -1 at the near plane up to 1 at the far plane.
struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
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
    s.x = (ndcX + 1.0f) * 0.5f * width;
    s.y = (1.0f - ndcY) * 0.5f * height;
    s.z = clip.z / clip.w;
    s.visible = true;
    return s;
}

// Projects each vertex once. A vertex is shared by several triangles, so this
// is much cheaper than projecting the corners of every triangle separately.
std::vector<ScreenPoint> projectVertices(const std::vector<Vec3>& positions, const Mat4& mvp,
                                         int width, int height) {
    std::vector<ScreenPoint> points;
    points.reserve(positions.size());
    for (const Vec3& p : positions) {
        points.push_back(projectToScreen(p, mvp, width, height));
    }
    return points;
}

// Brightness ramp, from dimmest to brightest: characters with more "ink" look
// brighter on a dark terminal background. A space isn't included, because a
// surface that's lit only by ambient light should still be visible.
constexpr char kShadeRamp[] = ".:-=+*#%@";
constexpr int kShadeLevels = sizeof(kShadeRamp) - 1; // minus the '\0'

// Light that reaches every surface, even ones facing away from the light.
// Without it, those surfaces would all be the same dimmest character.
constexpr float kAmbient = 0.15f;

// Maps a brightness from 0 to 1 onto the ramp.
char shadeChar(float intensity) {
    const int level = static_cast<int>(intensity * kShadeLevels);
    return kShadeRamp[std::clamp(level, 0, kShadeLevels - 1)];
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

// The "edge function": tells which side of the line a->b the point p is on.
// It's positive on one side, negative on the other, and zero on the line.
// Its value is also twice the signed area of the triangle (a, b, p), which is
// what makes it useful for barycentric coordinates below.
float edgeFunction(const ScreenPoint& a, const ScreenPoint& b, float px, float py) {
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

// Fills one triangle into the framebuffer, depth-tested.
// Returns false if the triangle was culled because it faces away from us.
bool fillTriangle(Framebuffer& fb, const ScreenPoint& a, const ScreenPoint& b,
                  const ScreenPoint& c, char ch) {
    // Signed area of the whole triangle. Our models list corners
    // counter-clockwise as seen from the front, but screen y points down,
    // which mirrors the picture: so front-facing triangles come out with
    // *negative* area here. Zero area means the triangle is seen edge-on.
    const float area = edgeFunction(a, b, c.x, c.y);
    if (area >= 0.0f) return false;

    // Only test cells inside the triangle's bounding box, clamped to the screen.
    const int minX = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int maxX = std::min(fb.width() - 1, static_cast<int>(std::floor(std::max({a.x, b.x, c.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int maxY = std::min(fb.height() - 1, static_cast<int>(std::floor(std::max({a.y, b.y, c.y}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            // Sample at the center of the cell.
            const float px = x + 0.5f;
            const float py = y + 0.5f;

            // Barycentric coordinates: how much each corner "owns" this point.
            // Each is the area of the small triangle opposite that corner,
            // divided by the whole area. They add up to 1, and they're all
            // >= 0 exactly when the point is inside the triangle. Dividing by
            // the (negative) area also flips the signs back to positive.
            const float w0 = edgeFunction(b, c, px, py) / area;
            const float w1 = edgeFunction(c, a, px, py) / area;
            const float w2 = edgeFunction(a, b, px, py) / area;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

            // Blend the corners' depths with the same weights. After the
            // divide by w, depth changes linearly across the screen, so this
            // simple blend gives the exact depth at this point.
            const float z = w0 * a.z + w1 * b.z + w2 * c.z;
            fb.setIfCloser(x, y, z, ch);
        }
    }
    return true;
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
    const std::vector<ScreenPoint> points =
        projectVertices(mesh.vertices, mvp, fb.width(), fb.height());

    auto edge = [&](int i, int j) {
        const ScreenPoint& a = points[i];
        const ScreenPoint& b = points[j];
        if (!a.visible || !b.visible) return;
        drawLine(fb, static_cast<int>(std::floor(a.x)), static_cast<int>(std::floor(a.y)),
                 static_cast<int>(std::floor(b.x)), static_cast<int>(std::floor(b.y)));
    };

    // Edges shared by two triangles get drawn twice. That's harmless here.
    for (const Triangle& t : mesh.triangles) {
        edge(t.a, t.b);
        edge(t.b, t.c);
        edge(t.c, t.a);
    }
}

int drawShaded(Framebuffer& fb, const Mesh& mesh, const Mat4& model,
               const Mat4& viewProjection, const Vec3& toLight) {
    // Move every vertex into world space once. Lighting is computed there,
    // and the same positions are then projected onto the screen.
    std::vector<Vec3> world;
    world.reserve(mesh.vertices.size());
    for (const Vec3& v : mesh.vertices) {
        world.push_back(model.transformPoint(v));
    }
    const std::vector<ScreenPoint> points =
        projectVertices(world, viewProjection, fb.width(), fb.height());

    const Vec3 lightDir = normalize(toLight);

    int drawn = 0;
    for (const Triangle& t : mesh.triangles) {
        const ScreenPoint& a = points[t.a];
        const ScreenPoint& b = points[t.b];
        const ScreenPoint& c = points[t.c];
        if (!a.visible || !b.visible || !c.visible) continue;

        // The face normal: cross two edges of the triangle. With corners in
        // counter-clockwise order, it points out of the front of the face.
        const Vec3& pa = world[t.a];
        const Vec3 normal = normalize(cross(world[t.b] - pa, world[t.c] - pa));

        // Lambert's cosine law: a surface facing the light head-on gets full
        // light, and it falls off with the cosine of the angle (the dot
        // product of two unit vectors). Facing away (negative) means no light.
        const float diffuse = std::max(0.0f, dot(normal, lightDir));
        const float intensity = kAmbient + (1.0f - kAmbient) * diffuse;

        if (fillTriangle(fb, a, b, c, shadeChar(intensity))) ++drawn;
    }
    return drawn;
}
