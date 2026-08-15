/// Test Scene construction and config access.
#include <test_macros.hpp>
#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "game/input/player_input_stream.hpp"
#include "oge/json.hpp"
#include "game/scene.hpp"
#include "game/sim/registry.hpp"
#include "game/terrain/terrain_view.hpp"
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

/// Standalone-simulation smoke test: a plain game::Scene (no server scene,
/// no client scene — the transport layer is optional) must run the full
/// default sim config with a live player.  The fixed SubsystemPlayer reads
/// sim::SimTickContext from world ctx; the Scene constructor guarantees one
/// in every scene world and Update advances it (tick arbitration), so the
/// stage's single tick-frame code path needs no transport-layer fallback.
TEST(scene_standalone_player_sim_smoke) {
    INIT

    game::Scene::Def def{appCtx, {}};
    game::Scene scene(def);

    game::sim::RegisterSubsystems(appCtx.any_factory);

    scene.GetConfig() = game::GetDefaultSceneConfig(types);
    scene.Load();

    auto& w = scene.GetWorld();
    auto player = game::ComponentPlayer::CreatePlayer(
        w, game::PlayerInfo{{}, {20.f, 20.f, 20.f}});
    CHECK(w.valid(player));

    // Drive local input like the view does: raw 60 Hz frames.  The scene
    // aggregates them into tick-stamped frames itself (the transport
    // pollers' standalone equivalent).
    {
        game::input::PlayerInputFrameDelta delta;
        delta.moveDelta = {1.f, 0.f};
        delta.dt = 1.f / 60.f;
        w.get<game::input::PlayerInputStream>(player).PushFrame(delta);
    }
    CHECK(w.get<game::input::PlayerInputStream>(player).IsLocalInput());

    std::optional<oge::runtime::oge_id_type> nextScene;
    game::json::Object nextSceneArgs;

    // One second at 60 fps: several fixed frames (30 Hz, 2 sub-steps each)
    // plus 60 realtime ticks.  The fixed pipeline runs
    // SubsystemPlayer<FixedStep> over the player every frame.
    const auto spawnPos = w.get<game::ComponentPhysicBody>(player).pos;
    for (int i = 0; i < 60; ++i)
    {
        scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
    }
    CHECK(w.valid(player));

    // The realtime trio simulates the player: gravity moved the body off
    // its spawn height (falling, or landed on terrain below the spawn).
    const auto& body = w.get<game::ComponentPhysicBody>(player);
    CHECK(body.pos.y < spawnPos.y);

    scene.Unload();
}

/// Standalone dig action: a bare game::Scene has no transport layer, so
/// PollPlayerActions never runs — the scene itself aggregates the local
/// input into tick-stamped frames (sim::AggregateLocalInputs) and the fixed
/// player stage drains them like it does on the server.  Without the
/// aggregation the 3-slot accumulator fills ("player action overflow"
/// warn) and dig/place actions never remove a block.
TEST(scene_standalone_player_dig_action) {
    INIT

    game::Scene::Def def{appCtx, {}};
    game::Scene scene(def);

    game::sim::RegisterSubsystems(appCtx.any_factory);

    scene.GetConfig() = game::GetDefaultSceneConfig(types);
    scene.Load();

    auto& w = scene.GetWorld();
    auto player = game::ComponentPlayer::CreatePlayer(
        w, game::PlayerInfo{{}, {20.f, 20.f, 20.f}});
    CHECK(w.valid(player));

    std::optional<oge::runtime::oge_id_type> nextScene;
    game::json::Object nextSceneArgs;
    auto step = [&](int frames)
    {
        for (int i = 0; i < frames; ++i)
            scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
    };
    auto pumpUntil = [&](auto&& done, int maxFrames)
    {
        for (int i = 0; i < maxFrames; ++i)
        {
            scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
            if (done()) return true;
        }
        return false;
    };

    // Settle: terrain generates under the spawn and the body lands.
    CHECK(pumpUntil(
        [&]
        {
            return w.get<game::ComponentPhysicBody>(player).isGrounded;
        },
        1200));

    // Aim straight down: ViewToRay(camera, {0, 0}) == camera forward.
    auto& cam = w.get<game::ComponentCamera>(player);
    cam.SetYawPitch(0.f, -game::math::radians(89.f));

    // The realtime stage only chases the camera while draining input — push
    // a no-op delta so one poll places the camera over the body.
    auto& stream = w.get<game::input::PlayerInputStream>(player);
    {
        game::input::PlayerInputFrameDelta delta;
        stream.PushFrame(delta);
    }
    step(1);

    // Precompute the expected dig hit by casting the SAME ray the realtime
    // stage emits: origin = the chased camera position, dir = forward.
    const game::math::vec3 rayOrigin = cam.position;
    const game::math::vec3 rayDir = cam.forward;
    auto& terrain = w.ctx().get<game::terrain::TerrainView>();
    const auto preHit = terrain.CastRay(rayOrigin, rayDir);
    CHECK(preHit.has_value());
    const game::Point3 hitPos = preHit->hitPos;
    {
        uint32_t preBlock = 0;
        CHECK(terrain.TryGetBlock(hitPos, preBlock));
        CHECK(preBlock != 0);
    }

    // Dig: the scene aggregates the action accumulation into a stamped
    // tick frame and the fixed stage applies the ray like the server path.
    {
        game::input::PlayerInputFrameDelta delta;
        delta.inputEvent = game::input::PlayerInputEvent{
            game::math::vec2{0.f, 0.f},
            game::input::PlayerActionKind::Digging};
        stream.PushFrame(delta);
    }

    CHECK(pumpUntil(
        [&]
        {
            uint32_t v = 0;
            return terrain.TryGetBlock(hitPos, v) && v == 0;
        },
        600));

    scene.Unload();
}

/// Standalone jump: jump input must take the same path as the networked
/// sim — raw frames aggregate into tick-stamped frames (the scene owns the
/// aggregation when no transport layer exists), the fixed player stage
/// reads the frame and raises the creature's jumpOrder, and the fixed
/// creature stage applies the impulse.  Before the sim-layer tick
/// aggregation the jump bit was dead input in standalone (the bare-world
/// fallback drained actions only), so the body never left the ground.
TEST(scene_standalone_player_jump) {
    INIT

    game::Scene::Def def{appCtx, {}};
    game::Scene scene(def);

    game::sim::RegisterSubsystems(appCtx.any_factory);

    scene.GetConfig() = game::GetDefaultSceneConfig(types);
    scene.Load();

    auto& w = scene.GetWorld();
    auto player = game::ComponentPlayer::CreatePlayer(
        w, game::PlayerInfo{{}, {20.f, 20.f, 20.f}});
    CHECK(w.valid(player));

    std::optional<oge::runtime::oge_id_type> nextScene;
    game::json::Object nextSceneArgs;
    auto pumpUntil = [&](auto&& done, int maxFrames)
    {
        for (int i = 0; i < maxFrames; ++i)
        {
            scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
            if (done()) return true;
        }
        return false;
    };

    // Settle: terrain generates under the spawn and the body lands.
    CHECK(pumpUntil(
        [&]
        {
            return w.get<game::ComponentPhysicBody>(player).isGrounded;
        },
        1200));

    const float groundY = w.get<game::ComponentPhysicBody>(player).pos.y;

    auto& stream = w.get<game::input::PlayerInputStream>(player);
    auto step = [&](int frames)
    {
        for (int i = 0; i < frames; ++i)
            scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
    };

    // Prime the aggregation: a fresh aggregate cursor snaps to the raw
    // ring frontier (the shared contract — a fresh reader starts at "now"),
    // so the very first frame ever pushed is sacrificed.  Push a no-op
    // delta and let one aggregation run so the jump frame below reads
    // normally (the same cold-start the networked pollers have).
    {
        game::input::PlayerInputFrameDelta delta;
        stream.PushFrame(delta);
    }
    step(3);

    // Jump: one raw frame with the Jump event.  The scene aggregates it
    // into a stamped tick frame on the next fixed frame; the fixed player
    // reads the frame (dueTick = tick - 1) and raises jumpOrder; the fixed
    // creature applies the impulse.
    {
        game::input::PlayerInputFrameDelta delta;
        delta.inputEvent = game::input::PlayerInputEvent{
            game::math::vec2{0.f, 0.f},
            game::input::PlayerActionKind::Jump};
        stream.PushFrame(delta);
    }

    // Scan ~2 s: the body must leave the ground and reach a jump apex.
    // Both the fixed and the realtime physics integrate gravity in the
    // standalone default config, so the effective gravity doubles
    // (~19.6 m/s²) and the 1.65 m jump-height impulse peaks at ~0.83 m.
    float maxY = groundY;
    bool lifted = false;
    for (int i = 0; i < 120; ++i)
    {
        scene.Update({1.f / 60.f}, {nextScene, nextSceneArgs});
        const float y = w.get<game::ComponentPhysicBody>(player).pos.y;
        if (y > maxY) maxY = y;
        if (y > groundY + 0.1f) lifted = true;
    }
    CHECK(lifted);
    CHECK(maxY > groundY + 0.4f);

    scene.Unload();
}

RUN_TESTS("Scene Tests")
