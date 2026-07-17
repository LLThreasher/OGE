#pragma once

#include <tuple>
#include <vector>

namespace oge
{

template <typename... TCommands>
struct SubmissionView
{
    std::tuple<std::vector<TCommands>&...> buckets;

    template <typename T>
    std::vector<T>& Get()
    {
        return std::get<std::vector<std::decay_t<T>>&>(buckets);
    }
};

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

    template <typename... Args>
    SubmissionView<Args...> View()
    {
        return {
            {Get<Args>()...}
        };
    }

    void Clear() { (std::get<std::vector<TCommands>>(buckets).clear(), ...); }
};

}  // namespace OneGame::Engine
