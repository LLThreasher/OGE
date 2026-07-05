#include <iostream>
#include <mutex>
#include <string>

#include <spdlog/sinks/base_sink.h>

#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/Logger.hpp"
#include "Engine/GameApp.hpp"
class Console
{
public:
    void UpdateInput(const std::string& input)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_input = input;
    }

    void PrintLog(spdlog::sinks::sink& wrapped,
                  const spdlog::details::log_msg& msg)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::cout << "\33[2K\r";   // clear line

        wrapped.log(msg);          // print log

        std::cout << "> " << m_input << std::flush;  // redraw once
    }
    std::mutex& Mutex() { return m_mutex; }

private:
    std::mutex m_mutex;
    std::string m_input;
};

template<typename Mutex>
class console_wrap_sink : public spdlog::sinks::base_sink<Mutex>
{
public:
    console_wrap_sink(Console& console,
                      std::shared_ptr<spdlog::sinks::sink> wrapped)
        : m_console(console), m_wrapped(std::move(wrapped))
    {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        m_console.PrintLog(*m_wrapped, msg);
    }

    void flush_() override
    {
        m_wrapped->flush();
    }

private:
    Console& m_console;
    std::shared_ptr<spdlog::sinks::sink> m_wrapped;
};

namespace OneGame::Engine
{
class CLIRunner : public IAppRunner<GameHeadlessApp>
{
   public:
    void Run(GameHeadlessApp& app);
};

void CLIRunner::Run(GameHeadlessApp& app)
{
    Console console;
    auto color_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    Logger::InitLogger("Engine", color_sink);

    app.Initialize();

    std::atomic<bool> running = true;
    
    std::thread inputThread([&]()
    {
        std::string buffer;

        while (running)
        {
            char c = std::cin.get();

            if (c == '\n')
            {
                if (!app.HandleCmd(buffer))
                    break;

                buffer.clear();
                console.UpdateInput(buffer);
            }
            else if (c == 127 || c == '\b')
            {
                if (!buffer.empty())
                    buffer.pop_back();

                console.UpdateInput(buffer);
            }
            else
            {
                buffer += c;
                console.UpdateInput(buffer);
            }
        }
    });

    HeadlessTickScheduler tick(60.0);
    LOG_INFO("starting main loop");
    while (true)
    {
        double dt = tick.WaitForNextTick();
        if (!app.Update(dt))
            break;
    }

    running = false;
    inputThread.join();

    app.Shutdown();
}

std::unique_ptr<IAppRunner<GameHeadlessApp>> CreateCLIRunner()
{
    return std::unique_ptr<IAppRunner<GameHeadlessApp>>(new CLIRunner);
}
}  // namespace OneGame::Engine