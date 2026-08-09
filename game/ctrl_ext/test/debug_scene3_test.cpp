/// Smoke test for DebugScene3 — verifies compilation, linkage, and type hierarchy.
#include "test_macros.hpp"

#include "game/debug_scene.hpp"
#include "game/app_context.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

TEST(type_register) {
    auto name = oge::runtime::TypeName<game::DebugScene3>::Get();
    CHECK_EQ(name, "core::DebugScene3");
}

TEST(inheritance) {
    bool ok1 = std::is_base_of_v<game::SceneExt, game::DebugScene3>;
    bool ok2 = std::is_base_of_v<game::Scene, game::DebugScene3>;
    bool ok3 = std::is_base_of_v<game::AppRuntime, game::DebugScene3>;
    CHECK(ok1);
    CHECK(ok2);
    CHECK(ok3);
}

TEST(default_scene_config) {
    entt::registry dummyReg;
    oge::runtime::OGEContext octx(dummyReg);
    oge::runtime::TypeRegistry types(octx);
    auto cfg = game::GetDefaultSceneConfig(types);
    CHECK(!cfg.subsystems.empty());
    CHECK(!cfg.realtimeSubsystems.empty());
    CHECK(cfg.loadMask & game::SceneConfig::LOAD_MASK_BLOCKS);
    CHECK(cfg.loadMask & game::SceneConfig::LOAD_MASK_TERRAIN);
}

RUN_TESTS("DebugScene3 Smoke Tests")
