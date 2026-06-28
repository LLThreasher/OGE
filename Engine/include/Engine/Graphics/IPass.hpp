#pragma once

namespace OneGame::Engine::Graphics
{
struct InitContext;
struct PrepareContext;
struct DrawContext;
class IGraphicsBackend;

template <typename... Args>
class IPass
{
   public:
    virtual ~IPass() = default;
    virtual void Enable(IGraphicsBackend& backend, InitContext& ctx, Args... args) = 0;
    virtual void Disable(IGraphicsBackend& backend) = 0;
    virtual void Prepare(PrepareContext& context) = 0;
    virtual void Draw(DrawContext& context) = 0;
};

class BasicPass : public IPass<>
{
};
}  // namespace OneGame::Engine::Graphics
