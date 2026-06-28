#pragma once
#include <memory>
#include <string>

namespace OneGame::Engine
{
class GameClientApp;

class IGameWindow
{
   public:
    virtual ~IGameWindow() = default;

    virtual void Run(GameClientApp& app) = 0;
};

std::unique_ptr<IGameWindow> CreateGameWindow(const std::string& title, int width, int height);
}  // namespace OneGame::Engine
