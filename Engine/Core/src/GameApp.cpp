#include "Engine/GameApp.hpp"

#include "Engine/TickScheduler.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
void GameHeadlessApp::Initialize()
{
    Parent::Initialize(m_appContext);
    m_cmdRunners.emplace("quit",
                         [&](auto cmd)
                         {
                             m_isrunning = false;
                             return false;
                         });
}

bool GameHeadlessApp::Update(float dt)
{
    m_dispatcher.update();
    FrameData frame{
        dt,
    };
    Parent::Update(m_appContext, frame);
    return m_isrunning;
}

bool GameHeadlessApp::HandleCmd(std::string_view cmd)
{
    auto it = m_cmdRunners.find(std::string(cmd));
    if (it != m_cmdRunners.end())
        return it->second(cmd);
    else
        LOG_WARN("unknown cmd: {}", cmd);
    return true;
}

void GameHeadlessApp::Shutdown() { Parent::Shutdown(m_appContext); }
}  // namespace OneGame::Engine
