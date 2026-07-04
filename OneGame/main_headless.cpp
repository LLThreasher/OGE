#include "Engine/GameApp.hpp"
#include "Engine/Platform/IGameWindow.hpp"
#include "Server.hpp"

using namespace OneGame;
using namespace OneGame::Engine;

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	auto runner = CreateCLIRunner();
	auto app = GameHeadlessApp();
	app.RegisterScene<DebugServer>();
	app.SwitchToScene<DebugServer>();
	runner->Run(app);
}
