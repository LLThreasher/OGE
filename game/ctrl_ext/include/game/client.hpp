#pragma once

#include "game/scene_view.hpp"
#include "game/scene_runner.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/asset_pool.hpp"
#include "oge/runtime/streaming_manager.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using namespace oge::platform;
using namespace oge::runtime;

class Client : public WindowApp, public SceneRunner<SceneView>
{
   public:
    Client();
    void Initialize(WindowHandle&) override;
    AppFrameAction Update(float dt, InputProvider pollInputs) override;
    void Shutdown() override;

    void OnWindowRecreate(WindowHandle&) override;
    void OnResize(int width, int height) override;

   private:
    oge::input::RawInputStream m_input;
    FramePerfStatus m_perfStats;

    IGraphicsBackend* m_backend;

    AssetManager& m_am;
    StreamingManager& m_sm;
    AssetPool& m_ap;

    DynamicChunkAllocator& m_ca;
    DynamicSkylineAllocator& m_sa;

    bool m_waitingSurface = false;

    bool BeginFrame(AppFrameAction& action);
    bool EndFrame(AppFrameAction& action);
};
}  // namespace game
