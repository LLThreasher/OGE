/**
 * Unit tests for SubsystemPhysics and AABB collision detection.
 *
 * Build: cmake --build . --target sim_physics_test
 * Run:   ctest -R sim_physics_test
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "game/components.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "oge/aabb.hpp"
#include "oge/aabb_ops.hpp"

static int g_passed = 0, g_failed = 0;
#define TEST(n) static void n(); struct R##n{R##n(){t.push_back({#n,n});}}r##n; static void n()
#define CHK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);++g_failed;return;}}while(0)
#define CHKEQ(a,b) do{if(!((a)==(b))){std::fprintf(stderr,"FAIL %s:%d: %s!=%s\n",__FILE__,__LINE__,#a,#b);++g_failed;return;}}while(0)
struct T{const char*n;void(*f)();};static std::vector<T> t;

// =========================================================================
// AABB collision tests
// =========================================================================

TEST(aabb_no_overlap) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{2,2,2}, oge::math::vec3{3,3,3}};
    CHK(!oge::CheckOverlap(a, b));
}

TEST(aabb_overlap) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{2,2,2}};
    oge::AABB b{oge::math::vec3{1,1,1}, oge::math::vec3{3,3,3}};
    CHK(oge::CheckOverlap(a, b));
}

TEST(aabb_edge_touching) {
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{1,1,1}, oge::math::vec3{2,2,2}};
    CHK(oge::CheckOverlap(a, b));  // touching at corner counts as overlap due to <=
}

TEST(collision_pos_x) {
    float off = 5.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{1.5f,0,0}, oge::math::vec3{2.5f,1,1}};
    CHK(oge::CheckCollision<0>(a, b, off, ct));
    CHKEQ(ct, oge::COLLISION_TYPE_NEG_X);  // moving right hits neg-x face of obstacle
    CHK(off < 5.f);
}

TEST(collision_neg_y) {
    float off = -5.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,1,0}, oge::math::vec3{1,2,1}};
    oge::AABB b{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    CHK(oge::CheckCollision<1>(a, b, off, ct));
    CHKEQ(ct, oge::COLLISION_TYPE_POS_Y);  // moving down hits pos-y face
    CHK(off > -5.f);
}

TEST(collision_no_movement) {
    float off = 0.f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{0.5f,0,0}, oge::math::vec3{1.5f,1,1}};
    CHK(!oge::CheckCollision<0>(a, b, off, ct));  // no movement = no collision response
    CHKEQ(off, 0.f);
}

TEST(collision_step_y_positive) {
    float off = 2.5f; int32_t ct = -1;
    oge::AABB a{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    oge::AABB b{oge::math::vec3{0,1.5f,0}, oge::math::vec3{1,2,1}};
    CHK(oge::CheckCollision<1>(a, b, off, ct));
    CHKEQ(ct, oge::COLLISION_TYPE_NEG_Y);  // moving up hits neg-y face of obstacle
    CHK(off < 2.5f);
}

// =========================================================================
// Component physics integration tests
// =========================================================================

TEST(phys_body_defaults) {
    game::ComponentPhysicBody body{};
    CHK(body.mass == 1.0f);
    CHK(body.enableGravity);
    CHK(!body.isGrounded);
}

TEST(phys_body_gravity_disabled) {
    game::ComponentPhysicBody body{};
    body.enableGravity = false;
    CHK(!body.enableGravity);
}

TEST(aabb_collider_defaults) {
    game::ComponentAABBCollider c{};
    // Default AABB should be at origin with zero size
    CHK(c.aabb.min == oge::math::vec3{});
    CHK(c.aabb.max == oge::math::vec3{});
}

TEST(aabb_collider_collision) {
    game::ComponentAABBCollider a{};
    a.aabb = oge::AABB{oge::math::vec3{0,0,0}, oge::math::vec3{2,2,2}};
    game::ComponentAABBCollider b{};
    b.aabb = oge::AABB{oge::math::vec3{1,1,1}, oge::math::vec3{3,3,3}};
    CHK(oge::CheckOverlap(a.aabb, b.aabb));
}

TEST(aabb_collider_no_collision) {
    game::ComponentAABBCollider a{};
    a.aabb = oge::AABB{oge::math::vec3{0,0,0}, oge::math::vec3{1,1,1}};
    game::ComponentAABBCollider b{};
    b.aabb = oge::AABB{oge::math::vec3{10,10,10}, oge::math::vec3{11,11,11}};
    CHK(!oge::CheckOverlap(a.aabb, b.aabb));
}

// =========================================================================
int main() {
    std::fprintf(stdout,"=== Physics Tests ===\n");
    for(auto& e:t){int b=g_failed;e.f();if(g_failed==b){++g_passed;std::fprintf(stdout,"  PASS %s\n",e.n);}}
    std::fprintf(stdout,"\nResults: %d passed, %d failed\n",g_passed,g_failed);
    return g_failed>0?EXIT_FAILURE:EXIT_SUCCESS;
}
