#include "game/server.hpp"
#include "oge/assert.hpp"
#include "oge/platform/spdlogger.hpp"
#include "oge/platform/stacktrace.hpp"

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

    SetLogger(new oge::platform::SpdLogger());
    oge::SetStackTraceFn(&oge::platform::PrintStackTrace);
    return game::Server().Run();
}
