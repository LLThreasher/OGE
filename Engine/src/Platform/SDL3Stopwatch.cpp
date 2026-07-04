#include "Engine/Platform/Stopwatch.hpp"
#ifdef USE_SDL3
#include <SDL3/SDL.h>

namespace OneGame::Engine
{

stopwatch stopwatch::start()
{
    stopwatch res;
    res.startTicks = SDL_GetPerformanceCounter();
    return res;
}

float stopwatch::restart()
{
    auto currentTicks = SDL_GetPerformanceCounter() - startTicks;
    startTicks = SDL_GetPerformanceCounter(); 
    return (float)currentTicks / (float)SDL_GetPerformanceFrequency() * 1000.f;
}

} // namespace OneGame::Engine
#endif
