#include "Engine/ECS/GraphicalComponents.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"


namespace OneGame::Engine::UI
{
    using namespace ECS;

    class BitmapFont16x6 : public IFont
    {
    public:
        ~BitmapFont16x6() = default;
        BitmapFont16x6(GPUTextureHandle texture, float textAspect) : m_texture(texture), m_textAspectRatio(textAspect) {}
        void CreateTextSprites(Graphics::SubmissionQueue& squeue, UIText text, ScreenRect rect) override;
    private:
        GPUTextureHandle m_texture;
        float m_textAspectRatio;
    };

    std::unique_ptr<IFont> LoadASCIIBitmapFont16x6(AssetContext& context, std::string_view textureId)
    {
        auto texture = context.LoadTexture(textureId);
        auto info = context.assetManager.GetTextureInfo(textureId);
        auto aspect = (float)(info->width / 16) / (float)(info->height / 6);
        return std::unique_ptr<IFont>(new BitmapFont16x6(texture, aspect));
    }

    void BitmapFont16x6::CreateTextSprites(Graphics::SubmissionQueue& squeue, UIText text, ScreenRect rect)
    {
        uint16_t ySize = text.size;
        uint16_t xSize = math::ceil((float)text.size * m_textAspectRatio);
        int16_t currentX = 0;
        int16_t currentY = 0;
        Graphics::PSprite currentSprite{m_texture};
        currentSprite.uv.extent = {1.f / 16.f, 1.f / 6.f};
        IRect16 currentRect;
        currentRect.extent = {xSize, ySize};
        auto it = text.text.begin();
        while (it != text.text.end() && (!text.enableWrap || currentX + xSize < rect.extent.x) && (text.enableCutoff || currentY + ySize < rect.extent.y))
        {
            unsigned char c = *it;
            ++it;
            if (c > 127) continue;
            if (c == '\n')
            {
                currentY += ySize;
                currentX = 0;
                continue;
            }
            if (c < 0x20) continue;
            c -= 0x20;
            currentSprite.uv.pos = {(float)(c % 16) / 16.f, (float)(c / 16) / 6.f };
            currentRect.pos = rect.pos + I16Point2{currentX, currentY};
            squeue.Add<Graphics::CmdDrawSprite>(Graphics::GameViewType::Overlay, currentRect, text.color, currentSprite);
            currentX += xSize;
        }
    }
}