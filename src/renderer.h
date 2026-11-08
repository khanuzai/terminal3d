#pragma once

#include "framebuffer.h"
#include "math3d.h"
#include "mesh.h"

// A terminal cell is roughly twice as tall as it is wide. The renderer uses
// this to keep shapes from looking stretched vertically.
constexpr float kCellAspect = 2.0f;

// Draws a straight line between two cells using Bresenham's algorithm.
// The character is picked from the line's direction: - | / or \.
void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1);

// Projects every vertex of the mesh with the model-view-projection matrix and
// draws the edges of every triangle.
void drawWireframe(Framebuffer& fb, const Mesh& mesh, const Mat4& mvp);

// Draws the mesh as solid, filled triangles. Triangles facing away from the
// camera are skipped (back-face culling) and the depth buffer hides surfaces
// behind other surfaces. Returns how many triangles were actually drawn.
int drawSolid(Framebuffer& fb, const Mesh& mesh, const Mat4& mvp);
