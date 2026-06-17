#pragma once

#include <string>

#ifndef LOGGER_NAME
#error "You must define LOGGER_NAME before including Logger.h"
#endif

#define LOG_INFO(...)  OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->info(__VA_ARGS__)
#define LOG_WARN(...)  OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->warn(__VA_ARGS__)
#define LOG_ERROR(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->error(__VA_ARGS__)

#ifdef _DEBUG
#define LOG_DEBUG(...) OneGame::Engine::Logger::GetLogger(LOGGER_NAME)->debug(__VA_ARGS__)
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
        static spdlog::logger* GetLogger(const char*);
    };
}