#pragma once

#include <string>

#ifndef LOGGER_NAME
#define LOGGER_NAME "Engine"
#endif

#define LOG_INFO(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->info(__VA_ARGS__)
#define LOG_WARN(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->warn(__VA_ARGS__)
#define LOG_ERROR(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->error(__VA_ARGS__)

#define LOG_DEBUG(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->debug(__VA_ARGS__)

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
    static void Initialize();
    static spdlog::logger* GetLogger(const char*);
};
}  // namespace OneGame::Engine