/// Test Scene construction and config access.
#include <test_macros.hpp>
#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "oge/json.hpp"
#include "game/scene.hpp"
#include "game/terrain/block_registry.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/typed_registry.hpp"

TEST(registry_bug_recreate_correct) {
using BlockEntry =
    std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;
    std::pmr::vector<BlockEntry> blocksEntries;
    blocksEntries = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    oge::runtime::OgeRegistry m_world;
    auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
    for (const auto& [name, config] : blocksEntries)
    {
        blocks.RegisterBlock(std::move(std::string(name)), config);
    }
}

TEST(registry_bug_recreate_ref) {
using BlockEntry =
    std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;
    std::pmr::vector<BlockEntry> blocksEntries;
    blocksEntries = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    oge::runtime::OgeRegistry reg;
    oge::runtime::OgeRegistryRef m_world{reg};
    auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
    for (const auto& [name, config] : blocksEntries)
    {
        blocks.RegisterBlock(std::move(std::string(name)), config);
    }
}

TEST(registry_bug_recreate_owned) {
using BlockEntry =
    std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;
    std::pmr::vector<BlockEntry> blocksEntries;
    blocksEntries = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    oge::runtime::OgeRegistry m_world;
    auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
    for (const auto& [name, config] : blocksEntries)
    {
        blocks.RegisterBlock(std::move(std::string(name)), config);
    }
}

TEST(registry_bug_recreate) {
using BlockEntry =
    std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;
    std::pmr::vector<BlockEntry> blocksEntries;
    blocksEntries = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    game::GameWorld m_world;
    auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
    for (const auto& [name, config] : blocksEntries)
    {
        blocks.RegisterBlock(std::move(std::string(name)), config);
    }
}

RUN_TESTS("Scene Tests")

