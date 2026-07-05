#pragma once
#include <memory>
#include <string>

namespace OneGame::Engine
{
class GameGraphicApp;
class GameHeadlessApp;

template <typename AppType>
class IAppRunner
{
   public:
    virtual ~IAppRunner() = default;

    virtual void Run(AppType& app) = 0;
};

std::unique_ptr<IAppRunner<GameGraphicApp>> CreateGameWindow(const std::string& title, int width, int height);
std::unique_ptr<IAppRunner<GameHeadlessApp>> CreateCLIRunner();
}  // namespace OneGame::Engine
