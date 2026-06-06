#pragma once
#include <memory>

#include <spdlog/spdlog.h>
#if defined(PLATFORM_ANDROID)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#define LOG_INFO(...)	Logger<LOGGER_NAME>::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)	Logger<LOGGER_NAME>::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)	Logger<LOGGER_NAME>::Get()->error(__VA_ARGS__)
#ifdef _DEBUG
#define LOG_DEBUG(...)	Logger<LOGGER_NAME>::Get()->debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...) {}
#endif

template <const char* NAME>
class Logger
{
public:
    static void Init()
    {
#if defined(__ANDROID__)
        auto sink = std::make_shared<spdlog::sinks::android_sink_mt>(NAME);
#else
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif
        s_EngineLogger = std::make_shared<spdlog::logger>(
            NAME,
            sink
        );

        s_EngineLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_EngineLogger);
        LOG_INFO("Logger initialized");
    }

    static std::shared_ptr<spdlog::logger>& Get()
    {
        if (s_EngineLogger == nullptr)
            Init();
        return s_EngineLogger;
    }

private:
    static inline std::shared_ptr<spdlog::logger> s_EngineLogger = nullptr;
};
