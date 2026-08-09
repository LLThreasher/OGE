#include "oge/log.hpp"

namespace oge
{

namespace
{
struct NullLogger : ILogger
{
    void Log(LogLevel, std::string_view) override {}
    void SetSink(SinkFn, void*) override {}
    void ClearSink() override {}
};
}  // namespace

static NullLogger g_nullLogger;
static ILogger* g_logger = &g_nullLogger;

void SetLogger(ILogger* logger)
{
    g_logger = logger ? logger : &g_nullLogger;
}

ILogger* GetLogger()
{
    return g_logger;
}
}  // namespace oge
