#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// =========================================================================
// Shared test macros — link against oge::test_support and include this
// header in any test file for uniform style.
//
// Usage:
//   #include <test_macros.hpp>
//   TEST(my_test) { CHECK(1 + 1 == 2); CHECK_EQ(x, 42); }
//   int main() { RUN_TESTS("My Suite"); }
// =========================================================================

#define TEST(name)                                                            \
    static void name();                                                       \
    struct R##name { R##name() { _g_tests.push_back({#name, name}); } } r##name; \
    static void name()

#define CHECK(expr)                                                           \
    do { if (!(expr)) { std::fprintf(stderr, "  FAIL %s:%d: %s\n",           \
                      __FILE__, __LINE__, #expr); ++_g_failed; return; }      \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do { if (!((a) == (b))) { std::fprintf(stderr, "  FAIL %s:%d: %s != %s\n", \
                       __FILE__, __LINE__, #a, #b); ++_g_failed; return; }    \
    } while (0)

struct TestEntry { const char* name; void (*fn)(); };

inline std::vector<TestEntry> _g_tests;
inline int _g_passed = 0;
inline int _g_failed = 0;

#define RUN_TESTS(title)                                                      \
    int main() {                                                              \
        std::fprintf(stdout, "=== %s ===\n", title);                          \
        for (auto& e : _g_tests) {                                            \
            int b = _g_failed; e.fn();                                        \
            if (_g_failed == b) { ++_g_passed;                                \
                std::fprintf(stdout, "  PASS %s\n", e.name); }                \
        }                                                                     \
        std::fprintf(stdout, "\nResults: %d passed, %d failed\n",             \
                     _g_passed, _g_failed);                                   \
        return _g_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;                   \
    }
