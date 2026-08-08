#pragma once

#include <unordered_map>

#include "game/view/gfx/commands.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/math.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/pass.hpp"

namespace game::view::gfx
{

using oge::runtime::gfx::Pass;
using oge::runtime::InitDrawContext;
using oge::runtime::DrawContext;
using oge::graphics::IGraphicsBackend;
using oge::graphics::BufferUsage;
using oge::runtime::FrameArena;
namespace math = ::oge::math;

// =========================================================================
// GeneralMeshPass
//
// Renders CmdDrawGeneralMeshOpaque commands with the mesh.{vert,frag}
// shaders.  Supports vertex color, UV texturing, and normal-based
// directional lighting.
//
// The pass binds:
//   set=0 binding=0: combined UBO (model + MVP + normal matrix)
//   set=0 binding=1: light UBO (direction, ambient, diffuse)
//   set=1 binding=0: texture sampler
// =========================================================================

class GeneralMeshPass : public RequiresVPTransform,
                        public Pass<CmdDrawGeneralMeshOpaque>
{
   public:
    struct MeshUBO
    {
        math::mat4 model;
        math::mat4 mvp;
        math::mat4 normalMatrix;
    };

    struct LightUBO
    {
        math::vec4 lightDir;     // xyz = direction, w unused
        math::vec4 ambientColor;
        math::vec4 lightColor;
    };

    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, const math::mat4& vpTransform);

    // Set per-frame lighting parameters.
    void SetLightDirection(const math::vec3& dir);
    void SetAmbientColor(const math::vec3& color);
    void SetLightColor(const math::vec3& color);

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(
        IGraphicsBackend& backend, GPUTextureHandle texture);

    std::unordered_map<GPUTextureHandle, std::vector<CmdDrawGeneralMeshOpaque>,
                       HandleHash<GPUTextureHandle>>
        m_drawCommands;

    std::unordered_map<GPUTextureHandle, GPUBindingGroupHandle,
                       HandleHash<GPUTextureHandle>>
        m_cachedBindingGroups;

    GPUPipelineHandle m_pipelineHandle;
    GPUBindingGroupLayoutHandle m_bindingLayout;
    FrameArena m_uniformArena{BufferUsage::Uniform};
    FrameArena m_lightArena{BufferUsage::Uniform};

    LightUBO m_light{};
};

}  // namespace game::view::gfx
