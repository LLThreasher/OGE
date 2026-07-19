#pragma once

namespace oge::input
{
enum TouchState
{
    None = 0,
    Pressed,
    Released,
    Moved,
    Cancelled
};

struct TouchPoint
{
    float x;
    float y;
};
}  // namespace OneGame::Engine
