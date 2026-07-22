#include "game/server.hpp"
#include <csignal>
#include "oge/log.hpp"
#include "oge/platform/spdlogger.hpp"

std::atomic<bool> keep_running(true);

void signal_handler(int signal_num)
{
    if (signal_num == SIGINT)
    {
        keep_running = false;
    }
}

namespace game {
    Server::Server(float tickInterval)
        : m_tick(tickInterval),
          m_ctx(m_metaWorld),
          m_am(*m_ctx.Emplace<AssetManager>()),
          SceneRunner(m_ctx)
    {
        SetLogger(new oge::platform::SpdLogger());
        using namespace sim;
        RegisterSubsystems(m_anyFactory);
    }

    int Server::Run()
    {
        std::signal(SIGINT, signal_handler);
        while (keep_running)
        {
            float dt = m_tick.WaitForNextTick();
            UpdateScene({dt});
        }
        LOG_INFO("Shutting down");
        DetachScene();
        return 0;
    }
}
