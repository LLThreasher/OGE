#pragma once
#include <memory>
#include <string>

namespace OneGame::Engine
{

template <typename AppType>
class IAppRunner
{
   public:
    virtual ~IAppRunner() = default;

    virtual void Run(AppType& app) = 0;
};

class GameHeadlessApp;
std::unique_ptr<IAppRunner<GameHeadlessApp>> CreateCLIRunner();
}  // namespace OneGame::Engine
