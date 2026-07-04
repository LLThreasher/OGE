#include "Engine/GameApp.hpp"
#include "Engine/Platform/IGameWindow.hpp"

using namespace OneGame::Engine;

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	auto runner = CreateCLIRunner();
	auto app = GameHeadlessApp();
	app.Initialize();
	runner->Run(app);
	app.Shutdown();
}
