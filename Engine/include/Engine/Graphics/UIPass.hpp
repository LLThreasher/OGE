#pragma once

#include <string>

#include "Engine/Graphics/IPass.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/entt.hpp"

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
   public:
    struct Position
    {
        uint16_t x;
        uint16_t y;
    };

    struct UV
    {
        uint16_t u;
        uint16_t v;

        static uint16_t Encode(float value)
        {
            value = std::clamp(value, 0.0f, 1.0f);
            return static_cast<uint16_t>(std::round(value * 65535.0f));
        }

        UV(float _u, float _v) : u(Encode(_u)), v(Encode(_v)) {}
    };

    struct Vertex
    {
        Position position;
        UV uv;
        ColorRGBA8 color;
        uint32_t textureIndex;
    };

    void Enable(IGraphicsBackend& backend, InitContext& ctxt) override;
    void Disable(IGraphicsBackend& backend) override;
    void Prepare(PrepareContext& ctx) override;
    void Draw(DrawContext& context) override;

   private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;
    std::vector<GPUTextureHandle> textures;
};
}  // namespace OneGame::Engine::Graphics