#pragma once
#include <array>
#include <cassert>
#include <memory_resource>
#include <vector>

namespace game
{

class CpuFrameArena
{
   public:
    CpuFrameArena(size_t bufferSize = 64 * 1024)
        : buffer_(bufferSize), resource_(buffer_.data(), buffer_.size())
    {
    }

    std::pmr::memory_resource* Resource() { return &resource_; }

    void Update(float dt)
    {
        resource_.release();  // frees all upstream blocks
    }

   private:
    std::vector<std::byte> buffer_;
    std::pmr::monotonic_buffer_resource resource_;
};

class CpuDurationFrameArena
{
   public:
    CpuDurationFrameArena(size_t size, float maxInterval)
        : arenaA(std::pmr::new_delete_resource()), arenaB(std::pmr::new_delete_resource()), switchInterval(maxInterval * 2)
    {
    }

    std::pmr::memory_resource* Resource() { return active ? &arenaA : &arenaB; }

    void Update(float dt)
    {
        elapsed += dt;

        if (elapsed >= switchInterval)
        {
            elapsed = 0.0;

            active = !active;

            // release the arena we're switching TO
            if (active)
                arenaA.release();
            else
                arenaB.release();
        }
    }

   private:
    std::pmr::monotonic_buffer_resource arenaA;
    std::pmr::monotonic_buffer_resource arenaB;

    bool active = false;
    float elapsed = 0.0;
    float switchInterval;
};

// class CpuSwapFrameArena
// {
//    public:
//     CpuSwapFrameArena(size_t framesInFlight, size_t bufferSize = 64 * 1024)
//         : framesInFlight(framesInFlight),
//           slot0(bufferSize),
//           slot1(framesInFlight >= 2 ? bufferSize : 0),
//           slot2(framesInFlight == 3 ? bufferSize : 0)
//     {
//         assert(framesInFlight <= 3);
//     }

//     std::pmr::memory_resource* Resource() { return currentFrameIdx == 0 ? &slot0 : (currentFrameIdx == 1 ? &slot1 : &slot2); }

//     void Update(float dt)
//     {
//         currentFrameIdx = (currentFrameIdx + 1) % framesInFlight;
//     }

//    private:
//     std::pmr::monotonic_buffer_resource slot0;
//     std::pmr::monotonic_buffer_resource slot1;
//     std::pmr::monotonic_buffer_resource slot2;
//     size_t currentFrameIdx = 0;
//     size_t framesInFlight = 1;
// };
class CpuSwapFrameArena
{
public:
    CpuSwapFrameArena(size_t framesInFlight,
                      size_t bufferSize = 64 * 1024)
        : framesInFlight(framesInFlight),
          arenas{
              std::pmr::monotonic_buffer_resource(bufferSize),
              std::pmr::monotonic_buffer_resource(bufferSize),
              std::pmr::monotonic_buffer_resource(bufferSize)
            //   std::pmr::monotonic_buffer_resource(framesInFlight >= 1 ? bufferSize : 0, std::pmr::null_memory_resource()),
            //   std::pmr::monotonic_buffer_resource(framesInFlight >= 2 ? bufferSize : 0, std::pmr::null_memory_resource()),
            //   std::pmr::monotonic_buffer_resource(framesInFlight == 3 ? bufferSize : 0, std::pmr::null_memory_resource())
          }
    {
        assert(framesInFlight > 0 && framesInFlight <= 3);
    }

    std::pmr::memory_resource* Resource()
    {
        // return std::pmr::new_delete_resource();
        return &arenas[currentFrameIdx];
    }

    void Update()
    {
        currentFrameIdx = (currentFrameIdx + 1) % framesInFlight;
        arenas[currentFrameIdx].release();
    }

    std::pmr::memory_resource* Get(size_t idx)
    {
        return &arenas[idx];
    }

    size_t Idx()
    {
        return currentFrameIdx;
    }

private:
    std::array<std::pmr::monotonic_buffer_resource, 3> arenas;
    size_t currentFrameIdx = 0;
    size_t framesInFlight;
};

struct MemoryContext
{
    CpuSwapFrameArena frameBuffer;
    CpuDurationFrameArena multiFrameBuffer;

    void Update(float dt)
    {
        frameBuffer.Update();
        multiFrameBuffer.Update(dt);
    }
};
}  // namespace game
