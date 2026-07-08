#pragma once

#include <string>

#include "Engine/Graphics/IPass.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/entt.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"

namespace OneGame::Engine::Graphics
{
enum class PivotType
{
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

enum class ScaleType
{
    Pixels,
    Percent,
    DPI,
};

enum class HAlign
{
    Left,
    Center,
    Right,
};

enum class VAlign
{
    Top,
    Center,
    Bottom,
};

struct RectBase
{
    PivotType pivotType;
    float x;
    float y;
    ScaleType scaleType;
    float width;
    float heigh;
    float opacity;
    int zIndex;
};

struct SpriteRect
{
    RectBase base;
    int textureId;
    ColorRGBA8 color;
    float bevelSize;
    bool nineSlice;
};

struct TextRect
{
    RectBase base;
    std::string text;
    int fontId;
    int fontSize;
    ColorRGBA8 textColor;
    HAlign horizontalAlign;
    VAlign verticalAlign;
    bool wrap;
};

class UIPass : public BasicPass
{
    struct PushConstant
    {
        math::mat2 transform;
        math::vec2 offset;
    };
    
   public:
    struct Vertex
    {
        U16Point2 position; // 4 byte
        U16Norm2 uv;        // 4 byte
        ColorRGBA8 color;   // 4 byte
    };

    void Enable(IGraphicsBackend& backend, InitContext& ctxt) override;
    void Disable(IGraphicsBackend& backend) override;
    void Draw(DrawContext& context) override;

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(IGraphicsBackend& backend, GPUTextureHandle texture);

    std::unordered_map<GPUTextureHandle, std::vector<Vertex>, HandleHash<GPUTextureHandle>> classedVertices;
    std::vector<uint16_t> indices;

    std::unordered_map<GPUTextureHandle, GPUBindingGroupHandle, HandleHash<GPUTextureHandle>> cachedBindingGroups;

    PushConstant pushConstant;
    
    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
};
}  // namespace OneGame::Engine::Graphics