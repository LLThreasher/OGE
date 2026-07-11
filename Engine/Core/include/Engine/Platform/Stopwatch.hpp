#pragma once
#include <cinttypes>

namespace OneGame::Engine
{
struct stopwatch
{
   public:
    static stopwatch start();
    float restart();

   private:
#ifdef USE_SDL3
    uint64_t startTicks;
#endif
};
}  // namespace OneGame::Engine