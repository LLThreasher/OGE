#pragma once
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "IPass.hpp"

namespace OneGame::Engine
{
struct AssetContext;
};
namespace OneGame::Engine::Graphics
{

constexpr uint32_t BLOCK_TEXTURE_SIZE = 16;

class UniformArena;
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

struct TerrainMesh
{
    uint32_t offset;
    uint32_t indexCount;
    Point3 coord;
};

class TerrainPass2 : public BasicPass
{
    struct UBO
    {
        math::mat4 mvp;
    };

   public:
    TerrainPass2() {}

    void UpdateBlockTexture(AssetContext& assets, const std::string& id, uint32_t slot);
    void Enable(IGraphicsBackend& backend, InitContext& ctxt) override;
    void Disable(IGraphicsBackend& backend) override;
    void Draw(DrawContext& context) override;

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(IGraphicsBackend& backend, UniformArena& arena,
                                                  GPUBufferHandle storageBuffer, uint32_t chunkSize);

    std::unordered_map<GPUBindingGroupHandle, std::vector<TerrainMesh>, HandleHash<GPUBindingGroupHandle>>
        activeChunkSlots;
    std::vector<UBO> ubos;

    std::unordered_map<GPUBufferHandle, GPUBindingGroupHandle, HandleHash<GPUBufferHandle>> cachedBindingGroups;

    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUTextureHandle blockTexture;
};
}  // namespace OneGame::Engine::Graphics
