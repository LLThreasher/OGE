#pragma once
#include "Engine/Point2.hpp"

namespace OneGame::Engine
{
struct IRect
{
    Point2 pos;
    UPoint2 extent;
};

struct FRect
{
    math::vec2 pos;
    math::vec2 extent;
};
}
