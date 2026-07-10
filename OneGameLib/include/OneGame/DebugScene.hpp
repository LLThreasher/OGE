#pragma once

#include <string>

#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/GameRenderer.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/IScene.hpp"

namespace OneGame
{
using namespace Engine;

const std::vector<Terrain::TexturedQuad> chunk_zero_quads = {
    {0, 0, 0, 0, 0, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
    {0, 0, 0, 1, 1, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
    {0, 0, 0, 2, 2, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
    {0, 0, 0, 3, 3, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
    {0, 0, 0, 4, 4, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
    {0, 0, 0, 5, 5, COLOR_WHITE, {0xF, 0xF, 0xF, 0xF}, {0, 0, 0, 0}},
};

const std::vector<Terrain::Vertex> chunk_zero_vertices = {
    // FRONT (+Z)
    {0, 0, 1, 0, 0xF, 0, 0, 0},
    {1, 0, 1, 0, 0xF, 0, 15, 0},
    {1, 1, 1, 0, 0xF, 0, 15, 15},
    {0, 1, 1, 0, 0xF, 0, 0, 15},

    // BACK (-Z)
    {1, 0, 0, 0, 0xF, 0, 0, 0},
    {0, 0, 0, 0, 0xF, 0, 15, 0},
    {0, 1, 0, 0, 0xF, 0, 15, 15},
    {1, 1, 0, 0, 0xF, 0, 0, 15},

    // LEFT (-X)
    {0, 0, 0, 0, 0xF, 0, 0, 0},
    {0, 0, 1, 0, 0xF, 0, 15, 0},
    {0, 1, 1, 0, 0xF, 0, 15, 15},
    {0, 1, 0, 0, 0xF, 0, 0, 15},

    // RIGHT (+X)
    {1, 0, 1, 0, 0xF, 0, 0, 0},
    {1, 0, 0, 0, 0xF, 0, 15, 0},
    {1, 1, 0, 0, 0xF, 0, 15, 15},
    {1, 1, 1, 0, 0xF, 0, 0, 15},

    // TOP (+Y)
    {0, 1, 1, 0, 0xF, 0, 0, 0},
    {1, 1, 1, 0, 0xF, 0, 15, 0},
    {1, 1, 0, 0, 0xF, 0, 15, 15},
    {0, 1, 0, 0, 0xF, 0, 0, 15},

    // BOTTOM (-Y)
    {0, 0, 0, 0, 0xF, 0, 0, 0},
    {1, 0, 0, 0, 0xF, 0, 15, 0},
    {1, 0, 1, 0, 0xF, 0, 15, 15},
    {0, 0, 1, 0, 0xF, 0, 0, 15},
};

const std::vector<uint16_t> chunk_zero_indices = {
    0,  1,  2,  2,  3,  0,   // front
    4,  5,  6,  6,  7,  4,   // back
    8,  9,  10, 10, 11, 8,   // left
    12, 13, 14, 14, 15, 12,  // right
    16, 17, 18, 18, 19, 16,  // top
    20, 21, 22, 22, 23, 20   // bottom
};

class DebugScene3 : public Scene<PresentationContext, const FrameInputData, FrameOutputData>
{
public:
    DebugScene3() : m_gameRenderer(m_gameWorld) {}
    virtual void Initialize(PresentationContext& context) override;
    virtual void Enter(PresentationContext& context) override;
    virtual void Exit(PresentationContext& context) override;
    virtual void Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut) override;
private:
    ECS::GameWorld m_gameWorld;
    ECS::GameRenderer m_gameRenderer;
};
}  // namespace OneGame::Engine
