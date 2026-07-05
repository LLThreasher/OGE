#include <cstdarg>
#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
spdlog::logger* Logger::GetLogger(const char* loggerName)
{
    auto existing = spdlog::get(loggerName);
    if (existing) return existing.get();

#if defined(__ANDROID__)
    auto sink = std::make_shared<spdlog::sinks::android_sink_mt>();
#else
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif

    auto logger = std::make_shared<spdlog::logger>(loggerName, sink);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);

    spdlog::register_logger(logger);

    return logger.get();
}
}  // namespace OneGame::Engine
