#pragma once

#include <cstdint>
#include <string>

#include "game/client_conn_scene.hpp"
#include "game/client_scene2.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/net/rollback_event_log_stream.hpp"
#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/server_scene.hpp"
#include "game/sim/registry.hpp"
#include "game/sim/subsystem.hpp"

namespace
{
constexpr uint16_t TEST_PORT = 23402;
constexpr float POLL_DT = 0.016f;
constexpr int MAX_POLLS = 500;  // ~8 sec at 60 fps
}  // namespace

// =============================================================================
// TestSceneRunner — wraps SceneRunner<Scene> with public scene access
// =============================================================================
class TestSceneRunner : public game::SceneRunner<game::Scene>
{
public:
    void Update(float dt) { UpdateScene({dt}); }

    game::Scene* GetScene() { return CurrentScene(); }
};

class TestClientScene : public game::ClientScene2
{
public:
    TestClientScene(const Def& def) : game::ClientScene2(def)
    {
        // normally this is done by scene view, here we do it like this for testing
        // we load client scene2
        game::ClientScene2::Load();
    }
};

DECL_TYPE_NAME(TestClientScene, "test::TestClientScene")

// =============================================================================
// NetSceneHarness — drives a server + client scene pair through ENet loopback
//
// The harness starts a DebugServerScene and ClientConnScene pair on TEST_PORT,
// polls both each tick, and exposes the scene worlds for test assertions.
// Handshake completion is detected by the presence of a ComponentPlayer entity
// in the server world (created by DebugServerScene when the client's PlayerInfo
// arrives).
// =============================================================================
struct NetSceneHarness
{
    TestSceneRunner m_serverRunner;
    TestSceneRunner m_clientRunner;
    bool m_clientPrediction = false;

    bool start()
    {
        // Register subsystem types with each runner's factory so scenes can
        // create SubsystemPlayer, SubsystemPhysics, etc. via AddStage.
        game::sim::RegisterSubsystems(m_serverRunner.AF());
        game::sim::RegisterSubsystems(m_clientRunner.AF());

        m_serverRunner.RegisterScene<game::DebugServerScene>();
        m_clientRunner.RegisterScene<game::ClientConnScene>();
        m_clientRunner.RegisterScene<game::ClientScene2>();
        m_clientRunner.RegisterScene<TestClientScene>();

        // Pass the test port to both scenes so they talk to each other.
        game::json::Object srvArgs;
        srvArgs["port"] = static_cast<int64_t>(TEST_PORT);
        m_serverRunner.SwitchToScene<game::DebugServerScene>(std::move(srvArgs));

        return true;
    }

    // Enable local physics/creature subsystems on the client so the
    // local player entity is simulated (client-side prediction).
    void enableClientPrediction()
    {
        m_clientPrediction = true;
    }

    bool connect()
    {
        game::json::Object cliArgs;
        cliArgs["port"] = static_cast<int64_t>(TEST_PORT);
        cliArgs["ip"] = std::string("127.0.0.1");
        cliArgs["next_scene"] =
            game::json::Int(entt::type_hash<TestClientScene>::value());

        game::SceneConfig cliConfig;
        cliConfig.loadMask = game::SceneConfig::LOAD_MASK_BLOCKS |
                             game::SceneConfig::LOAD_MASK_TERRAIN;
        cliConfig.blocks = {
            {"dirt", {"Dirt", "dirt.png", 1}},
            {"wood", {"Wood", "wood_plank.png", 1}},
            {"stone", {"Stone", "green_stone.png", 1}},
        };
        cliConfig.terrainDesc.chunkViewDistance = 4;

        // The stage lists come from the shared builders (Phase 2) — the
        // same config the real client synthesizes (ClientConnScene's
        // default).  Unconditional: every harness client runs the full
        // local sim (fixed trio + realtime trio + DebugText); the realtime
        // LocalPrediction filter keeps remote copies inert.  loadMask and
        // blocks only seed the terrain ctx — no terrain STAGE (a
        // client-side generator diverges from the server's replicated
        // chunks).
        game::sim::ApplyClientSimConfig(cliConfig, m_clientRunner.AF());

        cliArgs["scene_config"] = game::json::ToJson(cliConfig);
        m_clientRunner.SwitchToScene<game::ClientConnScene>(std::move(cliArgs));

        return true;
    }

    bool disconnect()
    {
        m_clientRunner.SwitchToScene<game::Scene>();

        return true;
    }

    // ---- polling ------------------------------------------------------------

    void poll()
    {
        m_serverRunner.Update(POLL_DT);
        m_clientRunner.Update(POLL_DT);
    }

    // ---- handshake detection ------------------------------------------------

    /// True when the server world contains at least one entity with
    /// ComponentPlayer (meaning the client's PlayerInfo was received and
    /// CreatePlayer ran successfully).
    bool isHandshakeDone()
    {
        auto* scene = m_serverRunner.GetScene();
        if (!scene) return false;
        auto& world = scene->GetWorld();
        for (auto [e, player] : world.view<game::ComponentPlayer>()->each())
        {
            (void)e;
            (void)player;
            return true;
        }
        return false;
    }

    /// Poll until `isHandshakeDone()` or maxPolls exhausted.
    bool waitForHandshake(int maxPolls = MAX_POLLS)
    {
        for (int i = 0; i < maxPolls; ++i)
        {
            poll();
            if (isHandshakeDone()) return true;
        }
        return false;
    }

    // ---- generic pump -------------------------------------------------------

    /// Poll both sides until `done()` returns true, bounded to maxPolls.
    template <typename Fn>
    bool pumpUntil(Fn&& done, int maxPolls = MAX_POLLS)
    {
        for (int i = 0; i < maxPolls; ++i)
        {
            poll();
            if (done()) return true;
        }
        return false;
    }

    // ---- world access -------------------------------------------------------

    game::Scene* serverScene() { return m_serverRunner.GetScene(); }
    game::Scene* clientScene() { return m_clientRunner.GetScene(); }

    game::GameWorld& serverWorld()
    {
        return m_serverRunner.GetScene()->GetWorld();
    }
    game::GameWorld& clientWorld()
    {
        return m_clientRunner.GetScene()->GetWorld();
    }

    // The client's authoritative mirror world (ClientScene2 variant 0).
    // Only valid after the client scene has switched to ClientScene2
    // (i.e. post-handshake, like clientWorld()).
    game::GameWorld& clientAuthoritativeWorld()
    {
        return *static_cast<game::ClientScene2&>(*m_clientRunner.GetScene())
            .GetAuthoritativeWorld();
    }

    // Access the client's RollbackEventLogStream for prediction assertions.
    // The stream lives in the authoritative world's ctx (it snapshots clean
    // server truth, not the predicted world).
    game::net::RollbackEventLogStream<>& clientRollbackStream()
    {
        return clientAuthoritativeWorld()
            .ctx()
            .get<game::net::RollbackEventLogStream<>>();
    }
};
