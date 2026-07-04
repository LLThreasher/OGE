#pragma once
#include <memory>
#include <string>

namespace OneGame::Engine
{
class GameGraphicApp;

class IGameWindow
{
   public:
    virtual ~IGameWindow() = default;

    virtual void Run(GameGraphicApp& app) = 0;
};

std::unique_ptr<IGameWindow> CreateGameWindow(const std::string& title, int width, int height);
}  // namespace OneGame::Engine
