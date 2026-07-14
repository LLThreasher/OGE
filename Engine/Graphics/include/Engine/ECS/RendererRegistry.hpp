#pragma once

#include "Engine/ECS/IRenderer.hpp"
#include "Engine/ECS/TypedRegistry.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"

namespace OneGame::Engine::ECS
{
class RendererRegistry : public TypedRegistry<RendererBase>
{
};

class RendererRegistryProvider
{
   protected:
    RendererRegistryProvider()
    {
        m_rendererRegistry.Register<DebugInfoRenderer>();
        m_rendererRegistry.Register<CameraRenderer>();
        m_rendererRegistry.Register<Terrain::TerrainRenderer>();
        m_rendererRegistry.Register<UIRenderer>();
        m_rendererRegistry.Register<PlayerInputRenderer>();
    }

    RendererRegistry m_rendererRegistry;
};
}  // namespace OneGame::Engine::ECS
