#pragma once

namespace OneGame::Engine
{
    class JobSystem {
    public:
        struct JobHandle {
            std::shared_ptr<std::atomic<bool>> cancelled;
            void cancel();
        };

        explicit JobSystem(size_t workerCount = 2);

        ~JobSystem();

        JobHandle submit(std::function<void(std::atomic<bool>&)> job);

        void shutdown();

    private:
        struct JobItem {
            std::function<void(std::atomic<bool>&)> func;
            std::shared_ptr<std::atomic<bool>> cancelFlag;
        };

        void workerLoop();

        std::vector<std::thread> workers;
        std::queue<JobItem> jobs;

        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop;
    };
}