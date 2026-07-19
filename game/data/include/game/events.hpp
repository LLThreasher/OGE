#include "oge/math.hpp"
#include "oge/point2.hpp"

namespace game
{
struct SurfaceRecreateEvent
{
    oge::U16Point2 swapchainExtent;
    oge::math::Orientation swapchainPretransform;
};
}
