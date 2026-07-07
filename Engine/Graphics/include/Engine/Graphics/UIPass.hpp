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
        U16Point2 position;
        U16Norm2 uv;
        ColorRGBA8 color;
        uint32_t textureIndex;
    };

    void Enable(IGraphicsBackend& backend, InitContext& ctxt) override;
    void Disable(IGraphicsBackend& backend) override;
    void Draw(DrawContext& context) override;

   private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    PushConstant pushConstant;
    
    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;
    std::array<GPUTextureHandle, 16> textures;
};
}  // namespace OneGame::Engine::Graphics