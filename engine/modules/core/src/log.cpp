#include "oge/log.hpp"

namespace oge
{
ILogger* g_logger;

void SetLogger(ILogger* logger)
{
    g_logger = logger;
}

ILogger* GetLogger()
{
    return g_logger;
}
}
