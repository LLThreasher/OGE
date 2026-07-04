#pragma once
#include "Engine/entt.hpp"
#include "Engine/GraphicState.hpp"
#include "GraphicalComponents.hpp"

#define DECLARE_RENDERER(Name, ...)                                                                       \
    class Name##Renderer : public RendererBase                                                           \
    {                                                                                                      \
       public:                                                                                             \
        void Initialize(GameWorldContext& game, PresentationContext& ctx) override;                        \
        void Present(const GameWorldContext& game, PresentationContext& ctx, FrameOutputData& fd) override;  \
                                                                                                           \
       private:                                                                                            \
        __VA_ARGS__                                                                                        \
    };

namespace OneGame::Engine::ECS
{
using GameWorldContext = entt::registry;
template <typename TData>
class IRenderer
{
   public:
    virtual ~IRenderer() = default;
    virtual void Initialize(TData& game, PresentationContext& ctx) = 0;
    virtual void Present(const TData& game, PresentationContext& ctx, FrameOutputData& fd) = 0;
};

class RendererBase : public IRenderer<GameWorldContext>
{
};

DECLARE_RENDERER(DebugInfo, float currentFPS = 0.f; float currentFrameTime = 0.f; float accumTime = 0.f;
                  uint64_t frameCount = 0; FramePerfStatus totalPerfStatus; FramePerfStatus perfStatus;);

void AddDebugInfo(entt::registry& presentationWorld, std::string_view msg);

DECLARE_RENDERER(Camera, void onViewPanelUpdate(entt::registry& world, entt::entity entity););
DECLARE_RENDERER(UI);

DECLARE_RENDERER(PlayerInput,
    void onCreateInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    bool isKeyMouseUsed;
    bool previousIsKeyMouseUsed;);

} // namespace OneGame::Engine
