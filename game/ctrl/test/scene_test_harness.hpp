#pragma once

#include "game/client_conn_scene.hpp"
#include "game/client_scene2.hpp"
#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/server.hpp"
#include "game/net/replication_registry.hpp"

namespace
{
constexpr uint16_t TEST_PORT = 23402;
constexpr float TEST_TIMEOUT_SEC = 5.0f;
constexpr float POLL_DT = 0.016f;
}  // namespace

class TestSceneRunner : public game::SceneRunner<game::Scene>
{
public:
    void Update(float dt)
    {
        UpdateScene({dt});
    }
};

struct NetSceneHarness
{
    TestSceneRunner m_serverRunner;
    TestSceneRunner m_clientRunner;

    bool serverReady = false;
    bool clientConnected = false;
    bool handshakeDone = false;

    // Traffic counters.  clientFamilies counts packets received by family id
    // (peeked before HandleIncoming consumes them), so tests can verify
    // delivery without PeekEvent — deserialized events are invisible to all
    // peers.
    int serverPackets = 0;
    int clientPackets = 0;
    std::unordered_map<game::net::oge_id_type, int> clientFamilies;

    bool start()
    {
        m_serverRunner.RegisterScene<game::DebugServerScene>();
        m_clientRunner.RegisterScene<game::ClientConnScene>();
        m_clientRunner.RegisterScene<game::ClientScene2>();

        m_serverRunner.SwitchToScene<game::DebugServerScene>();
        m_clientRunner.SwitchToScene<game::ClientConnScene>();

        return true;
    }

    void poll()
    {
        m_serverRunner.Update(POLL_DT);
    }

    bool waitForHandshake(float timeout = TEST_TIMEOUT_SEC)
    {
        float elapsed = 0.f;
        while (!handshakeDone)
        {
            poll();
            elapsed += POLL_DT;
            if (elapsed > timeout) return false;
        }
        return true;
    }

    // Poll both sides until `done` returns true, bounded to maxPolls so a
    // missed event fails the test instead of hanging the binary.
    template <typename Fn>
    bool pumpUntil(Fn&& done, int maxPolls = 200)
    {
        for (int i = 0; i < maxPolls; ++i)
        {
            poll();
            if (done()) return true;
        }
        return false;
    }
};
