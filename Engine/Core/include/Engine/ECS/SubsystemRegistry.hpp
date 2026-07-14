#pragma once
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/ECS/TypedRegistry.hpp"
#include "Engine/Terrain/TerrainService.hpp"

namespace OneGame::Engine::ECS
{
class SubsystemRegistry : public TypedRegistry<SubsystemBase>
{
};

class SubsystemRegistryProvider
{
   protected:
    SubsystemRegistryProvider()
    {
        m_subsystemRegistry.Register<SubsystemPlayerInput>();
        m_subsystemRegistry.Register<SubsystemUI>();
        m_subsystemRegistry.Register<SubsystemPlayer>();
        m_subsystemRegistry.Register<SubsystemPhysics>();
        m_subsystemRegistry.Register<SubsystemCreature>();
        m_subsystemRegistry.Register<Terrain::TerrainService>();
    }

    SubsystemRegistry m_subsystemRegistry;
};
}  // namespace OneGame::Engine::ECS
