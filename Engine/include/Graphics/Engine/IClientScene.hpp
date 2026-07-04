#pragma once

#include "Engine/GraphicState.hpp"

namespace OneGame::Engine
{
class ClientSceneBase
{
   public:
    virtual void Initialize(PresentationContext context) {}
    virtual void Enter(PresentationContext context) {}
    virtual void Exit(PresentationContext context) {}
    virtual void Shutdown(PresentationContext context) {}
    virtual void Update(PresentationContext context, const FrameInputData& fi, FrameOutputData& fo) {}
};
}  // namespace OneGame::Engine
