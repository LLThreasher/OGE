#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#if defined(PLATFORM_ANDROID)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include <memory>

#include "oge/log.hpp"

namespace oge::platform
{
class SpdLogger : public ILogger
{
   public:
    explicit SpdLogger()
    {
#if defined(__ANDROID__)
        auto sink = std::make_shared<spdlog::sinks::android_sink_mt>();
#else
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif

        m_logger = std::make_shared<spdlog::logger>("oge", sink);
        m_logger->sinks().push_back(m_ring_sink);
        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::trace);

        spdlog::register_logger(m_logger);
    }

    void Log(LogLevel level, std::string_view msg) override { m_logger->log(ToSpdLevel(level), msg); }

   private:
    static spdlog::level::level_enum ToSpdLevel(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace:
                return spdlog::level::trace;
            case LogLevel::Debug:
                return spdlog::level::debug;
            case LogLevel::Info:
                return spdlog::level::info;
            case LogLevel::Warn:
                return spdlog::level::warn;
            case LogLevel::Error:
                return spdlog::level::err;
            case LogLevel::Critical:
                return spdlog::level::critical;
        }
        return spdlog::level::info;  // fallback
    }

   private:
    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> m_ring_sink =
        std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt>(new spdlog::sinks::ringbuffer_sink_mt(128));
};
}  // namespace oge::platform
