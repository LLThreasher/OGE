#pragma once
#include "Engine/Point2.hpp"

namespace OneGame::Engine
{
template<typename TPos, typename TExtent>
struct Rect
{
    TPos pos;
    TExtent extent;
};

using IRect = Rect<Point2, UPoint2>;
using IRect16 = Rect<I16Point2, U16Point2>;
using URect16 = Rect<U16Point2, U16Point2>;
using FRect = Rect<math::vec2, math::vec2>;
using U16NormRect = Rect<U16Norm2, U16Norm2>;
}
