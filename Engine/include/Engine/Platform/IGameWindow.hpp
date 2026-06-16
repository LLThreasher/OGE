#pragma once
#include <string>
#include <memory>

namespace OneGame::Engine
{
    class GameApp;

    class IGameWindow
    {
    public:
        virtual ~IGameWindow() = default;

        virtual void Run(GameApp& app) = 0;
    };

    std::unique_ptr<IGameWindow>
        CreateGameWindow(const std::string& title,
            int width,
            int height);
}
