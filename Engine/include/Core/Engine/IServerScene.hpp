#pragma once

#include <string>

#include "Engine/GameAppState.hpp"

namespace OneGame::Engine
{

class ServerSceneBase
{
   public:
    virtual void Initialize(AppContext context) {}
    virtual void Enter(AppContext context) {}
    virtual void Exit(AppContext context) {}
    virtual void Shutdown(AppContext context) {}
    virtual void Update(AppContext context) {}
};

}  // namespace OneGame::Engine
