#include "Engine/PrintStackTrace.hpp"

#include "Engine/Logger.hpp"

#ifdef PLATFORM_WINDOWS
#pragma comment(lib, "dbghelp.lib")
#include <dbghelp.h>
#include <windows.h>

void PrintStackTrace()
{
    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < frames; i++)
    {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        LOG_ERROR("  {}: {}", i, symbol->Name);
    }

    free(symbol);
}
#elif defined(PLATFORM_ANDROID)
#include <dlfcn.h>
#include <unwind.h>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{

struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc)
    {
        if (state->current == state->end)
        {
            return _URC_END_OF_STACK;
        }
        else
        {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}

}  // namespace

size_t captureBacktrace(void** buffer, size_t max)
{
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - buffer;
}

void dumpBacktrace(std::ostream& os, void** buffer, size_t count)
{
    for (size_t idx = 0; idx < count; ++idx)
    {
        const void* addr = buffer[idx];
        const char* symbol = "";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname)
        {
            symbol = info.dli_sname;
        }

        os << "  #" << std::setw(2) << idx << ": " << addr << "  " << symbol << "\n";
    }
}

void PrintStackTrace()
{
    const size_t max = 64;
    void* buffer[max];
    std::ostringstream oss;

    dumpBacktrace(oss, buffer, captureBacktrace(buffer, max));

    LOG_ERROR("{}", oss.str().c_str());
}
#elif defined(PLATFORM_DARWIN)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

void PrintStackTrace()
{
    const int max_frames = 64;
    void* buffer[max_frames];

    int frame_count = backtrace(buffer, max_frames);
    char** symbols = backtrace_symbols(buffer, frame_count);

    for (int i = 0; i < frame_count; ++i) printf("%s\n", symbols[i]);

    free(symbols);
}
#else
void PrintStackTrace() {}
#endif
