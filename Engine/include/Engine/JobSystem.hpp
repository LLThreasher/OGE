#pragma once
#include "Engine/Async.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"

namespace OneGame::Engine
{
class JobSystem
{
   public:
    explicit JobSystem(size_t workerCount = 2);

    ~JobSystem();

    JobHandle submit(std::function<void(std::atomic<bool>*)> job);

    void shutdown();

   private:
    struct JobItem
    {
        std::function<void(std::atomic<bool>*)> func;
        JobHandle cancelHandle;
    };

    void workerLoop();

    ResourcePool<TempItem::Job, std::atomic<bool>> cancelFlags;
    std::mutex cancelMutex;

    std::vector<std::thread> workers;
    std::queue<JobItem> jobs;

    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
}  // namespace OneGame::Engine