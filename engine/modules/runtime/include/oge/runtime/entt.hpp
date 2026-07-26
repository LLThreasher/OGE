#pragma once

void handle_entt_assert_fail(const char* condition, const char* message,
                             const char* file, int line);

// EnTT's macro accepts two arguments: the condition and a descriptive text
// message
#define ENTT_ASSERT(condition, message)                                       \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            handle_entt_assert_fail(#condition, message, __FILE__, __LINE__); \
        }                                                                     \
    } while (0)

#include <entt/entt.hpp>
