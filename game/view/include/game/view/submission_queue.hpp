#pragma once

#include "game/view/gfx/commands.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/gfx/commands.hpp"
#include "oge/submission_group.hpp"

namespace game::view
{
using namespace oge;
using namespace oge::runtime;
using namespace oge::runtime::gfx;
using namespace ::game::view::gfx;

enum class GameViewType : uint32_t
{
    Overlay = 1 << 0,
    Slot0 = 1 << 1,
    Slot1 = 1 << 2,
    Slot2 = 1 << 3,
    Slot3 = 1 << 4,
    All = Overlay | Slot0 | Slot1 | Slot2 | Slot3,
};

struct ViewPanel
{
    GameViewType activeSlot = GameViewType::Slot0;
    entt::entity activeCamera = entt::null;
};

static constexpr std::array<GameViewType, 4> ALL_GAME_VIEWS = {
    GameViewType::Slot0,
    GameViewType::Slot1,
    GameViewType::Slot2,
    GameViewType::Slot3,
};

struct PGameViewTag
{
    GameViewType type;
};

struct PGameView : IRect
{
    GameViewType type;
};

template <typename SubmissionGroupT>
class ViewSubmissionGroup
{
   public:
    template <typename... Args>
    using TView = ViewSubmissionGroup<SubmissionView<Args...>>;
    static constexpr size_t ViewCount = std::bit_width(static_cast<uint32_t>(GameViewType::All));

    ViewSubmissionGroup() {}
    ViewSubmissionGroup(std::array<SubmissionGroupT, ViewCount> groups) : m_groups(std::move(groups)) {}

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

    template <typename... Args>
    TView<Args...> View()
    {
        // Passed the explicit template arguments and the required 'arr' argument
        return TView<Args...>(
            create_array_impl<Args...>(m_groups, std::make_index_sequence<ViewCount>{}));
    }

   private:
    static constexpr size_t BitToIndex(GameViewType viewBit)
    {
        return std::countr_zero(static_cast<uint32_t>(viewBit));
    }

    template <typename... Args, size_t... Is>
    static std::array<SubmissionView<Args...>, ViewCount> create_array_impl(
        std::array<SubmissionGroupT, ViewCount>& arr, std::index_sequence<Is...>)
    {
        // Corrected fold expansion and added 'template' keyword
        return {arr[Is].template View<Args...>()...};
    }

    std::array<SubmissionGroupT, ViewCount> m_groups;
};

using SingleSubmissionQueue = SubmissionGroup<CmdDrawSprite, CmdAddView, CmdDrawGeneralMeshOpaque,
                                              CmdDrawTerrainMeshOpaque, CmdDrawDebugText, CmdDrawDebugRect>;

class SubmissionQueue : public ViewSubmissionGroup<SingleSubmissionQueue>
{
};

}  // namespace game::view
