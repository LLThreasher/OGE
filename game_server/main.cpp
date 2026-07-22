#include "game/server.hpp"

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

    // std::pmr::set_default_resource(std::pmr::null_memory_resource());
    return game::Server().Run();
}
