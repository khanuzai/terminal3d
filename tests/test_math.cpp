#include <cmath>

#include "math3d.h"
#include "test.h"

namespace {

constexpr float kPi = 3.14159265f;
constexpr double kTol = 1e-5;

void checkVec(const Vec3& v, float x, float y, float z) {
    CHECK_NEAR(v.x, x, kTol);
    CHECK_NEAR(v.y, y, kTol);
    CHECK_NEAR(v.z, z, kTol);
}

} // namespace

TEST(cross_product_follows_right_hand_rule) {
    // x cross y = z, and swapping the order flips the sign.
    checkVec(cross({1, 0, 0}, {0, 1, 0}), 0, 0, 1);
    checkVec(cross({0, 1, 0}, {1, 0, 0}), 0, 0, -1);
    // The result is perpendicular to both inputs.
    const Vec3 a{1, 2, 3}, b{-4, 0.5f, 2};
    CHECK_NEAR(dot(cross(a, b), a), 0.0, kTol);
    CHECK_NEAR(dot(cross(a, b), b), 0.0, kTol);
}

TEST(normalize_gives_unit_length_and_leaves_zero_alone) {
    CHECK_NEAR(length(normalize({3, 4, 12})), 1.0, kTol);
    checkVec(normalize({0, 0, 0}), 0, 0, 0); // not NaN
}

TEST(rotations_turn_counter_clockwise_about_their_axis) {
    // Right-hand rule: a quarter turn about z takes x to y, about x takes y
    // to z, and about y takes z to x.
    checkVec(Mat4::rotationZ(kPi / 2).transformPoint({1, 0, 0}), 0, 1, 0);
    checkVec(Mat4::rotationX(kPi / 2).transformPoint({0, 1, 0}), 0, 0, 1);
    checkVec(Mat4::rotationY(kPi / 2).transformPoint({0, 0, 1}), 1, 0, 0);
}

TEST(matrix_product_applies_right_operand_first) {
    // In T * S, the point is scaled first and then moved.
    const Mat4 m = Mat4::translation({10, 0, 0}) * Mat4::scale(2);
    checkVec(m.transformPoint({1, 1, 1}), 12, 2, 2);
}

TEST(directions_ignore_translation) {
    const Mat4 m = Mat4::translation({5, 6, 7}) * Mat4::rotationZ(kPi / 2);
    checkVec(m.transformDirection({1, 0, 0}), 0, 1, 0);
    checkVec(m.transformPoint({1, 0, 0}), 5, 7, 7);
}

TEST(perspective_maps_near_and_far_planes_to_minus_one_and_one) {
    const Mat4 p = Mat4::perspective(kPi / 2, 1.0f, 0.5f, 20.0f);
    const Vec4 nearPoint = p * Vec4(Vec3{0, 0, -0.5f}, 1);
    const Vec4 farPoint = p * Vec4(Vec3{0, 0, -20}, 1);
    CHECK_NEAR(nearPoint.z / nearPoint.w, -1.0, kTol);
    CHECK_NEAR(farPoint.z / farPoint.w, 1.0, 1e-4);

    // With a 90 degree field of view, a point 45 degrees up lands on the top
    // edge of the screen (y = 1 after the divide), however far away it is.
    const Vec4 top = p * Vec4(Vec3{0, 3, -3}, 1);
    CHECK_NEAR(top.y / top.w, 1.0, kTol);
}
