#include "Engine/ECS/GraphicalComponents.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/AssetManager.hpp"


namespace OneGame::Engine::UI
{
    using namespace ECS;

    class BitmapFont16x6 : public IFont
    {
    public:
        BitmapFont16x6(GPUTextureHandle texture, float textAspect) : m_texture(texture), m_textAspectRatio(textAspect) {}
        void CreateTextSprites(entt::registry& world, UIText text, ScreenRect rect) override;
    private:
        GPUTextureHandle m_texture;
        float m_textAspectRatio;
    };

    std::unique_ptr<IFont> LoadASCIIBitmapFont16x6(AssetContext& context, std::string textureId)
    {
        auto texture = context.LoadTexture(textureId);
        auto info = context.assetManager.GetTextureInfo(textureId);
        auto aspect = (float)info->width / (float)info->height;
        return std::unique_ptr<IFont>(new BitmapFont16x6(texture, aspect));
    }

    void BitmapFont16x6::CreateTextSprites(entt::registry& world, UIText text, ScreenRect rect)
    {
        ushort ySize = text.size;
        ushort xSize = math::ceil((float)text.size * m_textAspectRatio);
        short currentX = 0;
        short currentY = 0;
        Graphics::PSprite currentSprite{m_texture};
        auto it = text.text.begin();
        while (it != text.text.end() && (!text.enableWrap || currentX + xSize < rect.extent.x) && (text.enableCutoff || currentY + ySize < rect.extent.y))
        {
            unsigned char c = *it;
            ++it;
            if (c > 127) continue;
            if (c == '\n')
            {
                currentY += ySize;
                continue;
            }
            if (c < 0x20) continue;
            currentSprite.uv.pos = {(float)(c % 16) / 16.f, (float)(c / 6) / 6.f };
            currentSprite.uv.extent = {1.f / 16.f, 1.f / 6.f};
            UISprite sp{.sprite = currentSprite, .color = text.color};
            ScreenRect currentRect;
            currentRect.pos = rect.pos + I16Point2{currentX, currentY};
            currentRect.extent = {xSize, ySize};
            auto e = world.create();
            world.emplace<UISprite>(e, currentSprite, text.color);
            world.emplace<ScreenRect>(e, currentRect);
            currentX += xSize;
        }
    }
}