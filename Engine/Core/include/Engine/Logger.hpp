#pragma once

#include <string>

#ifndef LOGGER_NAME
#define LOGGER_NAME "Engine"
#endif

#define LOG_INFO(...) Logger::GetLogger(LOGGER_NAME)->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::GetLogger(LOGGER_NAME)->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::GetLogger(LOGGER_NAME)->error(__VA_ARGS__)

#ifdef OGE_DEBUG
#define LOG_DEBUG(...) Logger::GetLogger(LOGGER_NAME)->debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#include <spdlog/spdlog.h>
#if defined(PLATFORM_ANDROID)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

namespace OneGame::Engine
{
class Logger
{
   public:
    template<typename Sink>
    static void InitLogger(const char* loggerName, std::shared_ptr<Sink> sink)
    {
        spdlog::drop(loggerName);
        auto logger = std::make_shared<spdlog::logger>(loggerName, sink);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);
        spdlog::register_logger(logger);
    }

    static spdlog::logger* GetLogger(const char*);
};
}  // namespace OneGame::Engine
using Logger = OneGame::Engine::Logger;