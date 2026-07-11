#pragma once

#include "Engine/ObjectType.hpp"
#include "Engine/Rect.hpp"

namespace OneGame::Engine
{

struct GPUTextureRegion
{
    URect region;
    GPUTextureHandle texture;
};

}  // namespace OneGame::Engine