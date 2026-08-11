#pragma once

#include "game/components.hpp"
#include "oge/aabb.hpp"
#include "oge/runtime/net_traits.hpp"

DECL_NET_OBJ(game::UpdateTag<game::UpdateType::FixedStep>, {})
DECL_NET_OBJ(game::UpdateTag<game::UpdateType::Realtime>, {})

DECL_NET_OBJ(game::ComponentCamera, {
    visit(self.position);
    visit(self.forward);
})

DECL_NET_OBJ(game::ComponentPerspectiveCamera, {
    visit(self.fov);
    visit(self.aspect);
})

DECL_NET_OBJ(game::ComponentCreature, {
    visit(self.maxSpeed);
    visit(self.initJumpSpeed);
})

DECL_NET_OBJ(oge::AABB, {
    visit(self.min);
    visit(self.max);
})

DECL_NET_OBJ(game::ComponentAABBCollider, {
    visit(self.aabb);
})

DECL_NET_OBJ(game::ComponentPlayer, {
    visit(self.id);
})

DECL_NET_OBJ(game::ComponentPhysicBody, {
    visit(self.pos);
    visit(self.mass);
    visit(self.stepAssist);
})
