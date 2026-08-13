#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

// =========================================================================
// Shared test macros — link against oge::test_support and include this
// header in any test file for uniform style.
//
// Usage:
//   #include <test_macros.hpp>
//   TEST(my_test) { CHECK(1 + 1 == 2); CHECK_EQ(x, 42); }
//   int main() { RUN_TESTS("My Suite"); }
//
// When oge/log.hpp is on the include path (the test binary links oge::core),
// a capturing logger is installed before each test and its output is dumped
// to stderr only when the test fails.
// =========================================================================

// ---- Optional log capture (requires oge::core) -------------------------
// Define INIT_LOGGER before including this header to enable per-test log
// capture.  Captured output is dumped to stderr only when the test fails.
// The test binary must link oge::core.
#ifdef INIT_LOGGER
#include "oge/log.hpp"
#include <sstream>

/// Captures log output during a test run.  On failure the captured lines
/// are dumped to stderr; on success they are silently discarded.
struct TestLogCapture : oge::ILogger
{
    std::ostringstream buffer;

    void Log(oge::LogLevel lvl, std::string_view msg) override
    {
        const char* prefix = "?";
        switch (lvl)
        {
            case oge::LogLevel::Trace:    prefix = "[T]"; break;
            case oge::LogLevel::Debug:    prefix = "[D]"; break;
            case oge::LogLevel::Info:     prefix = "[I]"; break;
            case oge::LogLevel::Warn:     prefix = "[W]"; break;
            case oge::LogLevel::Error:    prefix = "[E]"; break;
            case oge::LogLevel::Critical: prefix = "[C]"; break;
        }
        buffer << prefix << " " << msg << "\n";
    }

    void SetSink(oge::ILogger::SinkFn, void*) override {}
    void ClearSink() override {}

    void Dump()
    {
        auto s = buffer.str();
        if (!s.empty())
        {
            std::fprintf(stderr, "  --- captured logs ---\n%s"
                                "  --- end logs ---\n",
                         s.c_str());
        }
        Clear();
    }

    void Clear() { buffer.str(""); buffer.clear(); }
};

inline TestLogCapture& GetTestLogCapture()
{
    static TestLogCapture capture;
    return capture;
}

inline void InstallTestLogCapture()
{
    GetTestLogCapture().Clear();
    oge::SetLogger(&GetTestLogCapture());
}
#endif  // INIT_LOGGER

#ifndef EXTRACT_TESTS
#define TEST(name)                                           \
    static void name();                                      \
    struct R##name                                           \
    {                                                        \
        R##name()                                            \
        {                                                    \
            TestRegistry().emplace(#name, TestEntry{#name, name}); \
        }                                                    \
    } r##name;                                               \
    static void name()

#define CHECK(expr)                                                        \
    do                                                                     \
    {                                                                      \
        if (!(expr))                                                       \
        {                                                                  \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, \
                         #expr);                                           \
            ++_g_failed;                                                   \
            return;                                                        \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                 \
    do                                                                 \
    {                                                                  \
        if (!((a) == (b)))                                             \
        {                                                              \
            std::fprintf(stderr, "  FAIL %s:%d: %s != %s\n", __FILE__, \
                         __LINE__, #a, #b);                            \
            ++_g_failed;                                               \
            return;                                                    \
        }                                                              \
    } while (0)

struct TestEntry
{
    const char* name;
    void (*fn)();
};

// Registry is a function-local static so it is constructed on first use.
// An `inline` map here has unordered dynamic initialization (C++17
// [basic.start.dynamic]) — on clang-cl/MSVC the TEST registration
// constructors run before the map is built, crashing every test binary
// at startup.
inline std::unordered_map<std::string, TestEntry>& TestRegistry()
{
    static std::unordered_map<std::string, TestEntry> tests;
    return tests;
}

inline int _g_passed = 0;
inline int _g_failed = 0;

#define RUN_TEST_FN(e)                                                 \
    do                                                                 \
    {                                                                  \
        int b = _g_failed;                                             \
        InstallTestLogCapture();                                       \
        e.fn();                                                        \
        if (_g_failed != b) GetTestLogCapture().Dump();                \
    } while (0)

#define RUN_TESTS(title)                                                     \
    int main(int argc, char* argv[])                                         \
    {                                                                        \
        const char* filter = nullptr;                                        \
        for (int i = 1; i < argc; ++i)                                       \
        {                                                                    \
            const char* arg = argv[i];                                       \
            if (std::strncmp(arg, "--run-test=", 11) == 0)                   \
            {                                                                \
                filter = arg + 11;                                           \
                break;                                                       \
            }                                                                \
        }                                                                    \
        std::fprintf(stdout, "=== %s ===\n", title);                         \
        if (filter)                                                          \
        {                                                                    \
            auto it = TestRegistry().find(std::string(filter));                \
            if (it != TestRegistry().end())                                   \
            {                                                                \
                auto& e = it->second;                                        \
                int b = _g_failed;                                           \
                RUN_TEST_FN(e);                                              \
                if (_g_failed == b)                                          \
                {                                                            \
                    ++_g_passed;                                             \
                    std::fprintf(stdout, "  PASS %s\n", e.name);             \
                }                                                            \
            }                                                                \
            else                                                             \
            {                                                                \
                std::fprintf(stderr, "  FAIL unknown test: %s\n", filter);    \
                ++_g_failed;                                                 \
            }                                                                \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            for (auto& [_, e] : TestRegistry())                               \
            {                                                                \
                int b = _g_failed;                                           \
                RUN_TEST_FN(e);                                              \
                if (_g_failed == b)                                          \
                {                                                            \
                    ++_g_passed;                                             \
                    std::fprintf(stdout, "  PASS %s\n", e.name);             \
                }                                                            \
            }                                                                \
        }                                                                    \
        std::fprintf(stdout, "\nResults: %d passed, %d failed\n", _g_passed, \
                     _g_failed);                                             \
        return _g_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;                  \
    }

// No-op stubs when INIT_LOGGER is not defined — must appear before
// RUN_TEST_FN which references them.
#ifndef INIT_LOGGER
inline void InstallTestLogCapture() {}
struct TestLogCapture { void Dump() {} void Clear() {} };
inline TestLogCapture& GetTestLogCapture() { static TestLogCapture c; return c; }
#endif

#else  // EXTRACT_TESTS
#endif
