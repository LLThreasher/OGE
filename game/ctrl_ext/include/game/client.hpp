#pragma once

#include "game/graphical_scene.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/asset_pool.hpp"
#include "oge/runtime/streaming_manager.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/scene_runner.hpp"

namespace game
{
using namespace oge::platform;
using namespace oge::runtime;

class Client : public WindowApp, public SceneRunner<GraphicalScene>
{
   public:
    Client();
    void Initialize(WindowHandle*) override;
    AppFrameAction Update(float dt, InputProvider pollInputs) override;
    void Shutdown() override;

    void OnWindowRecreate(WindowHandle*) override;
    void OnResize(int width, int height) override;

   private:
    oge::input::RawInputStream m_input;
    FramePerfStatus m_perfStats;
    entt::registry m_metaWorld;
    OGEContext m_ctx;

    IGraphicsBackend* m_backend;

    AssetManager& m_am;
    StreamingManager& m_sm;
    AssetPool& m_ap;

    DynamicChunkAllocator& m_ca;
    DynamicSkylineAllocator& m_sa;

    bool m_waitingSurface = false;
};
}  // namespace game
