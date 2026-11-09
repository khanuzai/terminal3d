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

// Draws the mesh as solid, lit triangles.
//   model:          places the mesh in the world (where the light lives)
//   viewProjection: camera and perspective
//   toLight:        direction from the surface toward the light, in world space
// Triangles facing away from the camera are skipped (back-face culling) and
// the depth buffer hides surfaces behind other surfaces. Each triangle gets a
// character from a brightness ramp based on how directly it faces the light.
// Returns how many triangles were actually drawn.
int drawShaded(Framebuffer& fb, const Mesh& mesh, const Mat4& model,
               const Mat4& viewProjection, const Vec3& toLight);
