#pragma once

#include "framebuffer.h"
#include "math3d.h"
#include "mesh.h"

// Draws a straight line of pixels between two points using Bresenham's algorithm.
void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, const Color& color);

// Projects every vertex of the mesh with the model-view-projection matrix and
// draws the edges of every triangle.
void drawWireframe(Framebuffer& fb, const Mesh& mesh, const Mat4& mvp, const Color& color);

// Draws the mesh as solid, lit triangles.
//   model:          places the mesh in the world (where the light lives)
//   viewProjection: camera and perspective
//   toLight:        direction from the surface toward the light, in world space
//   baseColor:      the model's color when fully lit
// Triangles facing away from the camera are skipped (back-face culling) and
// the depth buffer hides surfaces behind other surfaces. Each triangle is
// shaded by how directly it faces the light.
// Returns how many triangles were actually drawn.
int drawShaded(Framebuffer& fb, const Mesh& mesh, const Mat4& model,
               const Mat4& viewProjection, const Vec3& toLight, const Color& baseColor);
