#include "game/server.hpp"
#include "oge/platform/spdlogger.hpp"

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

    SetLogger(new oge::platform::SpdLogger());
    // std::pmr::set_default_resource(std::pmr::null_memory_resource());
    return game::Server().Run();
}
