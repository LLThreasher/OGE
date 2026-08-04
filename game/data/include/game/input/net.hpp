#pragma once

#include "game/input/player_input_stream.hpp"
#include "game/input/entity_event_stream.hpp"
#include "oge/runtime/net_traits.hpp"

DECL_NET_OBJ(game::input::PlayerInputEvent, {
    visit(self.actionPos);
    visit(self.actionMask);
})

DECL_NET_OBJ(game::input::PlayerInputFrame, {
    visit(self.inputEvents);
    visit(self.moveDelta);
    visit(self.panDelta);
})
