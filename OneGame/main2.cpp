#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/GameApp.hpp"
#include "Engine/Platform/IGameWindow.hpp"
#include "TestRenderer.hpp"

using namespace OneGame;
using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;


int main() {
	auto window = CreateGameWindow("OneGame", 1280, 720);
	auto app = GameApp();

	window->Run(app);
}
