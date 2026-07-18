#pragma once

#include "oge/platform/window_app.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/scene.hpp"

namespace game
{
using namespace oge::platform;
using namespace oge::runtime;

class Client : public WindowApp
{
public:
    Client() : m_ctx(m_metaWorld), m_anyFactory(m_ctx) {}
    void Initialize(WindowHandle*);
    AppFrameAction Update(float dt, oge::input::RawInputStream& input);
    void Shutdown();

    void OnWindowRecreate(WindowHandle*);
    void OnResize(int width, int height);
private:
    AnythingFactory m_anyFactory;
    entt::registry m_metaWorld;
    OGEContext m_ctx;

    std::unique_ptr<IGraphicsBackend> m_backend;
    std::unique_ptr<Scene> m_scene;
};
}  // namespace game
