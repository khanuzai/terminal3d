#pragma once

#include "framebuffer.h"
#include "math3d.h"
#include "mesh.h"

// Draws a straight line of pixels between two points using Bresenham's algorithm.
void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, const Color& color);

// Projects every vertex of the mesh with the model-view-projection matrix and
// draws the edges of every triangle.
void drawWireframe(Framebuffer& fb, const Mesh& mesh, const Mat4& mvp, const Color& color);

// How drawShaded() lights and draws a mesh.
struct ShadeSettings {
    Vec3 toLight;    // direction from the surface toward the light, in world space
    Color baseColor; // the model's color when fully lit

    // false: skip triangles whose back faces the camera (back-face culling).
    //        Right for closed meshes, where back faces are always hidden anyway.
    // true:  draw them too, lit as if their normal pointed the other way.
    //        Needed for open meshes, where you can see through holes to the
    //        inside surface.
    bool twoSided = false;
};

// Draws the mesh as solid, lit triangles.
//   model:          places the mesh in the world (where the light lives)
//   viewProjection: camera and perspective
// The depth buffer hides surfaces behind other surfaces, and each triangle is
// shaded by how directly it faces the light.
// Returns how many triangles were actually drawn.
int drawShaded(Framebuffer& fb, const Mesh& mesh, const Mat4& model,
               const Mat4& viewProjection, const ShadeSettings& settings);
