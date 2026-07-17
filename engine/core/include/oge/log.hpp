#pragma once

#include <string>
#include <format>

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
    Info,
    Warn,
    Error,
#ifdef OGE_DEBUG
    Debug,
#endif
};

class ILogger
{
   public:
    virtual ~ILogger() = default;
    virtual void Log(LogLevel level, std::string_view msg) = 0;
};

void SetLogger(ILogger*);
ILogger* GetLogger();

template<typename... Args>
void Log(LogLevel lvl, std::format_string<Args...> fmt, Args&&... args)
{
    auto msg = std::format(fmt, std::forward<Args>(args)...);
    GetLogger()->Log(lvl, msg);
}

void Log(LogLevel lvl, std::string_view msg)
{
    GetLogger()->Log(lvl, msg);
}

}  // namespace oge
