#pragma once

#include <string_view>
#include <fmt/format.h>

#define LOG_INFO(...) oge::Log(oge::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) oge::Log(oge::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) oge::Log(oge::LogLevel::Error, __VA_ARGS__)

#ifdef OGE_DEBUG
#define LOG_DEBUG(...) oge::Log(oge::LogLevel::Debug, __VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

namespace oge
{
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
};

class ILogger
{
   public:
    using SinkFn = void(*)(LogLevel, std::string_view, void* user);

    virtual ~ILogger() = default;
    virtual void Log(LogLevel level, std::string_view msg) = 0;
    virtual void SetSink(SinkFn fn, void* user) = 0;
    virtual void ClearSink() = 0;
};

void SetLogger(ILogger*);
ILogger* GetLogger();

template<typename... Args>
void Log(LogLevel lvl, fmt::format_string<Args...> fmt, Args&&... args)
{
    auto msg = fmt::format(fmt, std::forward<Args>(args)...);
    GetLogger()->Log(lvl, msg);
}

static void Log(LogLevel lvl, std::string_view msg)
{
    GetLogger()->Log(lvl, msg);
}

}  // namespace oge
