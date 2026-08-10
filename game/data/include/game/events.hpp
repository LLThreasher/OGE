#pragma once

#include "oge/math.hpp"
#include "oge/point2.hpp"

struct _ENetPeer;
typedef _ENetPeer ENetPeer;

namespace game
{
struct SurfaceRecreateEvent
{
    oge::U16Point2 swapchainExtent;
    oge::math::Orientation swapchainPretransform;
};

struct WindowResizeEvent
{
    oge::U16Point2 windowSize;
};

struct OnAddPeer
{
    ENetPeer* peer;
    uint32_t peerId;
};

struct OnRemovePeer
{
    uint32_t peerId;
};
}  // namespace game
