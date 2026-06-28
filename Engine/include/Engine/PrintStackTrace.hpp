#pragma once

#ifdef PLATFORM_WINDOWS
#include <dbghelp.h>
#include <windows.h>
#pragma comment(lib, "dbghelp.lib")

static void PrintStackTrace()
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
#endif
