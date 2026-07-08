#pragma once

#include <cstddef>
#include <string>

#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Rect.hpp"
#include "Engine/ECS/SubmissionGroup.hpp"

namespace OneGame::Engine::Graphics
{
struct CmdDrawGeneralMeshOpaque
{
    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    uint32_t indexCount;
};

struct CmdDrawTerrainMeshOpaque
{
    PTerrainMesh terrainMesh;
    Point3 coords;
};

struct CmdDrawDebugText
{
    std::string text;
    ColorRGBA8 color;
};

struct CmdDrawDebugRect
{
    IRect16 rect;
    ColorRGBA8 color;
};

struct CmdDrawSprite
{
    IRect16 rect;
    ColorRGBA8 color;
    PSprite sprite;
};

struct CmdAddView
{
    IRect16 rect;
    math::mat4 view = math::lookAt(math::vec3(20, 20, 20), math::vec3(0, 0, 0), math::vec3(0, 1, 0));
    float fov = math::radians(45.0f);
    float aspect = 0.f;
};

template <typename SubmissionGroupT>
class ViewSubmissionGroup
{
   public:
    static constexpr size_t ViewCount = std::bit_width(static_cast<uint32_t>(GameViewType::All));

    // Single-bit access
    SubmissionGroupT& GetSingle(GameViewType viewBit) { return m_groups[BitToIndex(viewBit)]; }

    // Mask access
    class MaskView
    {
       public:
        MaskView(SubmissionGroupT* groups, uint32_t mask) : m_groups(groups), m_mask(mask) {}

        template <typename Fn>
        void ForEach(Fn&& fn)
        {
            uint32_t mask = m_mask;

            while (mask)
            {
                uint32_t bit = mask & -mask;
                size_t index = std::countr_zero(bit);

                fn(m_groups[index]);

                mask &= ~bit;
            }
        }

       private:
        SubmissionGroupT* m_groups;
        uint32_t m_mask;
    };

    MaskView Get(GameViewType views) { return MaskView(m_groups.data(), static_cast<uint32_t>(views)); }

    template <typename T, typename... Args>
    void Add(GameViewType views, Args&&... args)
    {
        uint32_t mask = static_cast<uint32_t>(views);

        while (mask)
        {
            uint32_t bit = mask & -mask;
            size_t index = std::countr_zero(bit);

            m_groups[index].template Add<T>(std::forward<Args>(args)...);

            mask &= ~bit;
        }
    }

    void Clear()
    {
        for (auto& g : m_groups) g.Clear();
    }

   private:
    static constexpr size_t BitToIndex(GameViewType viewBit) { return std::countr_zero(static_cast<uint32_t>(viewBit)); }

    std::array<SubmissionGroupT, ViewCount> m_groups;
};

using SingleSubmissionQueue = SubmissionGroup<CmdDrawSprite, CmdAddView, CmdDrawGeneralMeshOpaque, CmdDrawTerrainMeshOpaque, CmdDrawDebugText, CmdDrawDebugRect>;
using SubmissionQueue = ViewSubmissionGroup<SingleSubmissionQueue>;
}  // namespace OneGame::Engine::Graphics
