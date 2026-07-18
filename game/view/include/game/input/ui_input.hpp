#include "game/input/input_srouce.hpp"

namespace game::input
{

    class InputSourceUI : InputSource
    {
        void onAttach(InputContext& ctx) override;
        void onDetach(InputContext& ctx) override;
        void onUpdate(FInputContext& ctx) override;
    };
} // namespace game::input
