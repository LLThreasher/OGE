#pragma once

#include <cstdlib>
#include <string>

#include "oge/log.hpp"

// =========================================================================
// OGE_ASSERT  —  debug-mode assertion with file:line and a formatted
//                message.  A stack trace hook can be installed at runtime;
//                if set, it is called before aborting.
//
// In release builds the check is compiled out entirely.
//
// The stack trace interface lives here so that core code can use
// OGE_ASSERT.  The actual implementation is registered by the platform
// module at startup.
// =========================================================================

#ifdef OGE_DEBUG

#define OGE_ASSERT(condition, ...)                                            \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            ::oge::detail::AssertFail(#condition, __FILE__, __LINE__,         \
                                      ::fmt::format(__VA_ARGS__));           \
        }                                                                     \
    } while (0)

#define OGE_ASSERT_MSG(condition, msg)                                        \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            ::oge::detail::AssertFail(#condition, __FILE__, __LINE__, msg);  \
        }                                                                     \
    } while (0)

#else

#define OGE_ASSERT(condition, ...)    ((void)0)
#define OGE_ASSERT_MSG(condition, msg) ((void)0)

#endif

namespace oge
{

// Callback installed by the platform module to print a stack trace.
// If nullptr (the default), stack traces are simply skipped.
using StackTraceFn = void (*)();

void SetStackTraceFn(StackTraceFn fn);
StackTraceFn GetStackTraceFn();

namespace detail
{

inline void AssertFail(const char* condition, const char* file, int line,
                       const std::string& msg)
{
    LOG_ERROR("========================================");
    LOG_ERROR("ASSERTION FAILED");
    LOG_ERROR("  {}:{}", file, line);
    LOG_ERROR("  condition: {}", condition);
    if (!msg.empty()) LOG_ERROR("  message:   {}", msg);
    LOG_ERROR("========================================");

    auto st = GetStackTraceFn();
    if (st) st();

    std::abort();
}

}  // namespace detail
}  // namespace oge
