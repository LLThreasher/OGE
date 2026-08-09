/**
 * Unit tests for SubsystemPhysics and AABB collision detection.
 *
 * Build: cmake --build . --target sim_physics_test
 * Run:   ctest -R sim_physics_test
 */

#include <test_macros.hpp>
#include <vector>

#include "game/components.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "oge/aabb.hpp"
#include "oge/aabb_ops.hpp"


// =========================================================================
// AABB collision tests
// =========================================================================

TEST(aabb_no_overlap) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{2,2,2}, oge::math::vec3{3,3,3}};
    CHECK(!oge::CheckOverlap(a, b));
}

TEST(aabb_overlap) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{2,2,2}};
    oge::AABB b{oge::math::vec3{1,1,1}, oge::math::vec3{3,3,3}};
    CHECK(oge::CheckOverlap(a, b));
}

TEST(aabb_edge_touching) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{1,1,1}, oge::math::vec3{2,2,2}};
    CHECK(oge::CheckOverlap(a, b));  // touching at corner counts as overlap due to <=
}

TEST(collision_pos_x) {
    float off = 5.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{1.5f,0,0}, oge::math::vec3{2.5f,1,1}};
    CHECK(oge::CheckCollision<0>(a, b, off, ct));
    CHECK_EQ(ct, oge::COLLISION_TYPE_NEG_X);  // moving right hits neg-x face of obstacle
    CHECK(off < 5.f);
}

TEST(collision_neg_y) {
    float off = -5.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,1,0}, oge::math::vec3{1,2,1}};
    oge::AABB b{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    CHECK(oge::CheckCollision<1>(a, b, off, ct));
    CHECK_EQ(ct, oge::COLLISION_TYPE_POS_Y);  // moving down hits pos-y face
    CHECK(off > -5.f);
}

TEST(collision_no_movement) {
    float off = 0.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{0.5f,0,0}, oge::math::vec3{1.5f,1,1}};
    CHECK(!oge::CheckCollision<0>(a, b, off, ct));  // no movement = no collision response
    CHECK_EQ(off, 0.f);
}

TEST(collision_step_y_positive) {
    float off = 2.5f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{0,1.5f,0}, oge::math::vec3{1,2,1}};
    CHECK(oge::CheckCollision<1>(a, b, off, ct));
    CHECK_EQ(ct, oge::COLLISION_TYPE_NEG_Y);  // moving up hits neg-y face of obstacle
    CHECK(off < 2.5f);
}

// =========================================================================
// Component physics integration tests
// =========================================================================

TEST(phys_body_defaults) {
    game::ComponentPhysicBody body{};
    CHECK(body.mass == 1.0f);
    CHECK(body.enableGravity);
    CHECK(!body.isGrounded);
}

TEST(phys_body_gravity_disabled) {
    game::ComponentPhysicBody body{};
    body.enableGravity = false;
    CHECK(!body.enableGravity);
}

TEST(aabb_collider_defaults) {
    game::ComponentAABBCollider c{};
    // Default AABB should be at origin with zero size
    CHECK(c.aabb.min == oge::math::vec3{});
    CHECK(c.aabb.max == oge::math::vec3{});
}

TEST(aabb_collider_collision) {
    game::ComponentAABBCollider a{};
    a.aabb = oge::AABB{oge::math::vec3{0,0,0}, oge::math::vec3{2,2,2}};
    game::ComponentAABBCollider b{};
    b.aabb = oge::AABB{oge::math::vec3{1,1,1}, oge::math::vec3{3,3,3}};
    CHECK(oge::CheckOverlap(a.aabb, b.aabb));
}

TEST(aabb_collider_no_collision) {
    game::ComponentAABBCollider a{};
    a.aabb = oge::AABB{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    game::ComponentAABBCollider b{};
    b.aabb = oge::AABB{oge::math::vec3{10,10,10}, oge::math::vec3{11,11,11}};
    CHECK(!oge::CheckOverlap(a.aabb, b.aabb));
}

// =========================================================================
RUN_TESTS("Physics Tests")
