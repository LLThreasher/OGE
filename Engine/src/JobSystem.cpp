#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include "Engine/JobSystem.hpp"

namespace OneGame::Engine
{
    void JobSystem::JobHandle::cancel()
    {
        if (cancelled)
            cancelled->store(true, std::memory_order_relaxed);
    }

    JobSystem::JobSystem(size_t workerCount)
        : stop(false)
    {
        for (size_t i = 0; i < workerCount; ++i) {
            workers.emplace_back([this]() { workerLoop(); });
        }
    }

    JobSystem::~JobSystem() {
        shutdown();
    }

    JobSystem::JobHandle JobSystem::submit(std::function<void(std::atomic<bool>&)> job) {
        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            jobs.push({ job, cancelFlag });
        }

        condition.notify_one();
        return JobHandle{ cancelFlag };
    }

    void JobSystem::shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();

        for (auto& t : workers) {
            if (t.joinable())
                t.join();
        }
    }

    void JobSystem::workerLoop() {
        while (true) {
            JobItem item;

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this]() {
                    return stop || !jobs.empty();
                    });

                if (stop && jobs.empty())
                    return;

                item = std::move(jobs.front());
                jobs.pop();
            }

            // Skip if cancelled before execution
            if (!item.cancelFlag->load(std::memory_order_relaxed)) {
                item.func(*item.cancelFlag);
            }
        }
    }
}