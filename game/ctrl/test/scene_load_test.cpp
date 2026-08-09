/// Test Scene construction and config access.
#include <test_macros.hpp>
#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene.hpp"
#include "game/sim/registry.hpp"
#include "oge/runtime/typed_registry.hpp"

#define INIT \
    oge::runtime::OgeRegistry rawReg; \
    oge::runtime::OGEContext octx(rawReg); \
    oge::runtime::TypeRegistry types(octx); \
    entt::dispatcher events; \
    game::MemoryContext mem{{}, {64 * 1024, 1.f}, {64 * 1024, 0.1f}}; \
    game::AppContext appCtx{octx, types, events, mem};

TEST(scene_one_step_update) {
    INIT

    game::Scene::Def def{appCtx, {}};
    game::Scene scene(def);

    game::sim::RegisterSubsystems(appCtx.any_factory);

    auto& w = scene.GetWorld();
    CHECK(!w.valid(entt::entity{0}));

    auto& cfg = scene.GetConfig();
    cfg = game::GetDefaultSceneConfig(types);
    CHECK(cfg.loadMask & game::SceneConfig::LOAD_MASK_BLOCKS);
    CHECK(!cfg.subsystems.empty());

    std::optional<oge::runtime::oge_id_type> nextScene;
    game::json::Object nextSceneArgs;

    scene.Load();
    scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
    scene.Unload();
}

RUN_TESTS("Scene Tests")
