#pragma once

namespace oge::platform
{
struct RAMInfo
{
    unsigned long long RSS = 0;
    unsigned long long NativeHeapBlks = 0;
    unsigned long long NativeHeapReserved = 0;
};

RAMInfo GetRAMUsage();
double GetCPUUsage();
double GetGPUUsage();
} // namespace OGE::Platform
