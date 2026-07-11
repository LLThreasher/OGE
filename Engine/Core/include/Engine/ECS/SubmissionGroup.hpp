#pragma once

#include <tuple>
#include <vector>

#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine
{
template <typename... TCommands>
struct SubmissionGroup
{
    std::tuple<std::vector<TCommands>...> buckets;

    template <typename T, typename... Args>
    void Add(Args&&... args)
    {
        auto& bucket = std::get<std::vector<T>>(buckets);
        bucket.emplace_back(std::forward<Args>(args)...);
    }

    template <typename T>
    std::vector<T>& Get()
    {
        return std::get<std::vector<std::decay_t<T>>>(buckets);
    }

    void Clear() { (std::get<std::vector<TCommands>>(buckets).clear(), ...); }
};

}  // namespace OneGame::Engine
