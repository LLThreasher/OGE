#include "Engine/Platform.hpp"

#include "Engine/Logger.hpp"

#ifdef PLATFORM_WINDOWS
// clang-format off
#pragma comment(lib, "dbghelp.lib")
#include <windows.h>
#include <dbghelp.h>
// clang-format on

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

#include <chrono>
#include <mach/mach.h>

void PrintStackTrace()
{
    const int max_frames = 64;
    void* buffer[max_frames];

    int frame_count = backtrace(buffer, max_frames);
    char** symbols = backtrace_symbols(buffer, frame_count);

    for (int i = 0; i < frame_count; ++i) printf("%s\n", symbols[i]);

    free(symbols);
}

double GetGPUUsage() { return -1.0; }

// Returns the resident memory (RAM) used by the current process in bytes
unsigned long long GetRAMUsage() {
    task_basic_info_data_t info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;
}

// Returns the CPU usage percentage of the current process (100.0 means 1 core is fully utilized)
double GetCPUUsage() {
    task_thread_times_info_data_t thread_times;
    mach_msg_type_number_t thread_times_count = TASK_THREAD_TIMES_INFO_COUNT;
    
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&thread_times, &thread_times_count) != KERN_SUCCESS) {
        return -1.0;
    }

    // Step 1: Query all individual threads belonging to this task
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    if (task_threads(mach_task_self(), &thread_list, &thread_count) != KERN_SUCCESS) {
        return -1.0;
    }

    long total_user_sec = 0, total_user_usec = 0;
    long total_system_sec = 0, total_system_usec = 0;

    // Step 2: Sum up CPU time slices across all live threads
    for (mach_msg_type_number_t i = 0; i < thread_count; ++i) {
        thread_basic_info_data_t thread_info_data;
        mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
        
        if (thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)&thread_info_data, &thread_info_count) == KERN_SUCCESS) {
            // Check if thread is not currently idling out
            if (!(thread_info_data.flags & TH_FLAGS_IDLE)) {
                total_user_sec += thread_info_data.user_time.seconds;
                total_user_usec += thread_info_data.user_time.microseconds;
                total_system_sec += thread_info_data.system_time.seconds;
                total_system_usec += thread_info_data.system_time.microseconds;
            }
        }
        mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list, thread_count * sizeof(thread_t));

    // Combine seconds and microseconds
    double total_time = total_user_sec + total_system_sec + (total_user_usec + total_system_usec) / 1000000.0;
    
    // Step 3: Differentiate time over a known interval to get percentage
    static double last_time = 0.0;
    static auto last_clock = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    double elapsed_wall_time = std::chrono::duration<double>(now - last_clock).count();
    
    if (elapsed_wall_time == 0) return 0.0;

    double cpu_usage = ((total_time - last_time) / elapsed_wall_time) * 100.0;

    // Save history for next calculation cycle
    last_time = total_time;
    last_clock = now;

    return cpu_usage;
}

#else
void PrintStackTrace() {}
unsigned long long GetRAMUsage() { return 0; }
double GetCPUUsage() { return -1.0; }
double GetGPUUsage() { return -1.0; }
#endif
