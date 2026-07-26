#include "oge/runtime/entt.hpp"

#include <cstdlib>

#include "oge/log.hpp"
#include "oge/platform/stacktrace.hpp"

void handle_entt_assert_fail(const char* condition, const char* message,
                             const char* file, int line)
{
    LOG_ERROR("[EnTT assert failure]");
    LOG_ERROR("Condition: {}", condition);
    LOG_ERROR("Message: {}", message);
    LOG_ERROR("Location: {}:{}", file, line);
    oge::platform::PrintStackTrace();
    std::abort();
}
