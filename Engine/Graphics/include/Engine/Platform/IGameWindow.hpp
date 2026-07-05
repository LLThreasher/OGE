#pragma once
#include <memory>
#include <string>
#include "Engine/Platform/IAppRunner.hpp"

namespace OneGame::Engine
{
class GameGraphicApp;
std::unique_ptr<IAppRunner<GameGraphicApp>> CreateGameWindow(const std::string& title, int width, int height);
}  // namespace OneGame::Engine
