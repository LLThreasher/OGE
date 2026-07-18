#include "game/input/input_srouce.hpp"

namespace game::input
{
    class PlayerInputKeyMouse : InputSource
    {
        void onAttach(InputContext& ctx) override;
        void onDetach(InputContext& ctx) override;
        void onUpdate(FInputContext& ctx) override;
    };

    class PlayerInputWidget : InputSource
    {
        void onAttach(InputContext& ctx) override;
        void onDetach(InputContext& ctx) override;
        void onUpdate(FInputContext& ctx) override;
    };
} // namespace game::input
