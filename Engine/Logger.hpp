#pragma once
#include <memory>

#include <spdlog/spdlog.h>
#if defined(PLATFORM_ANDROID)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#define LOG_INFO(...)  Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#ifdef _DEBUG
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...) {}
#endif

class Logger
{
public:
    static void Init()
    {
#if defined(__ANDROID__)
        auto sink = std::make_shared<spdlog::sinks::android_sink_mt>("ENGINE");
#else
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif
        s_EngineLogger = std::make_shared<spdlog::logger>(
            "ENGINE",
            sink
        );

        s_EngineLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_EngineLogger);
    }

    static std::shared_ptr<spdlog::logger>& Get()
    {
        return s_EngineLogger;
    }

private:
    static inline std::shared_ptr<spdlog::logger> s_EngineLogger;
};
