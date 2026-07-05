#include "Engine/JobSystem.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace OneGame::Engine
{
JobSystem::JobSystem(size_t workerCount) : stop(false)
{
    for (size_t i = 0; i < workerCount; ++i)
    {
        workers.emplace_back([this]() { workerLoop(); });
    }
}

JobSystem::~JobSystem() { shutdown(); }

JobHandle JobSystem::submit(std::function<void(std::atomic<bool>*)> job)
{
    JobHandle res;
    {
        std::lock_guard lock(cancelMutex);
        res = cancelFlags.Create(false);
    }

    {
        std::lock_guard lock(queueMutex);
        jobs.push({std::move(job), res});
    }

    condition.notify_one();
    return res;
}

void JobSystem::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();

    for (auto& t : workers)
    {
        if (t.joinable()) t.join();
    }
}

void JobSystem::workerLoop()
{
    while (true)
    {
        JobItem job;

        {
            std::unique_lock lock(queueMutex);
            condition.wait(lock, [this] { return stop || !jobs.empty(); });

            if (stop && jobs.empty()) return;

            job = std::move(jobs.front());
            jobs.pop();
        }

        auto flag = cancelFlags.Get(job.cancelHandle);
        job.func(flag);
        cancelFlags.Destroy(job.cancelHandle);
    }
}
}  // namespace OneGame::Engine