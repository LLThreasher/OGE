#pragma once
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "IPass.hpp"

namespace OneGame::Engine::Graphics
{

class TerrainPass : public IPass<Mesh>
{
    struct UBO
    {
        math::mat4 mvp;
    };

   public:
    TerrainPass() : orientation(math::identity_quat()) {}

    void Enable(IGraphicsBackend& backend, InitContext& ctxt, Mesh terrainMesh) override;
    void Disable(IGraphicsBackend& backend) override;
    void Prepare(PrepareContext& context) override;
    void Draw(DrawContext& context) override;
    void SetPalette(ColorRGBA8 colors[16]);

   private:
    GPUBufferHandle colorPaletteGpuBuffer;
    void* colorPaletteStagingBuffer;
    bool isColorPaletteDirty = true;

    Mesh terrainMesh = {};
    std::vector<PTerrainMesh> activeChunkSlots;
    std::vector<UBO> ubos;

    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;
    GPUTextureHandle blockTexture;

    math::quat orientation;
};

class TerrainPass2 : public IPass<GPUBufferHandle>
{
    struct UBO
    {
        math::mat4 mvp;
    };

   public:
    TerrainPass2() {}

    void Enable(IGraphicsBackend& backend, InitContext& ctxt, GPUBufferHandle terrainMesh) override;
    void Disable(IGraphicsBackend& backend) override;
    void Prepare(PrepareContext& context) override;
    void Draw(DrawContext& context) override;

   private:
    GPUBufferHandle storageBuffer;
    std::vector<PTerrainMesh> activeChunkSlots;
    std::vector<UBO> ubos;

    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;
    GPUTextureHandle blockTexture;
};
}  // namespace OneGame::Engine::Graphics
