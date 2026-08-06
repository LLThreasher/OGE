#pragma once

#include <cassert>
#include <deque>
#include <tuple>
#include <vector>

#include "oge/macros.hpp"

namespace oge
{

template <typename T>
using SubmissionList = std::pmr::deque<T>;

template <typename... TCommands>
struct SubmissionView
{
    std::tuple<SubmissionList<TCommands>&...> buckets;

    SubmissionView(std::tuple<SubmissionList<TCommands>&...> buckets)
        : buckets(buckets)
    {
    }

    template <typename T>
    SubmissionList<T>& Get()
    {
        return std::get<SubmissionList<std::decay_t<T>>&>(buckets);
    }

    template <typename T, typename... Args>
    void Add(Args&&... args)
    {
        auto& bucket = std::get<SubmissionList<T>&>(buckets);
        bucket.emplace_back(std::forward<Args>(args)...);
    }
};

template <typename T>
struct VectorWrapper
{
    using Resource = std::pmr::memory_resource;
    SubmissionList<T> vec;

    void clear(Resource* r)
    {
        size_t size = vec.size();
        vec = SubmissionList<T>{r};
        // vec.reserve(size);
    }
};

template <typename... TCommands>
struct SubmissionGroup
{
    using Resource = std::pmr::memory_resource;
    template <typename... Args>
    using TView = SubmissionView<Args...>;

    SubmissionGroup() = delete;
    SubmissionGroup(Resource* r)
        : resource(r), buckets({SubmissionList<TCommands>{r}}...)
    {
    }
    SubmissionGroup(const SubmissionGroup&) = delete;
    SubmissionGroup& operator=(const SubmissionGroup&) = delete;

    std::tuple<VectorWrapper<TCommands>...> buckets;
    Resource* resource;

    template <typename T, typename... Args>
    void Add(Args&&... args)
    {
        auto& bucket = std::get<VectorWrapper<T>>(buckets).vec;
        assert(bucket.get_allocator().resource());
        bucket.emplace_back(std::forward<Args>(args)...);
    }

    template <typename T>
    SubmissionList<T>& Get()
    {
        return std::get<VectorWrapper<std::decay_t<T>>>(buckets).vec;
    }

    template <typename... Args>
    SubmissionView<Args...> View()
    {
        return {{Get<Args>()...}};
    }

    void Clear()
    {
        (std::get<VectorWrapper<TCommands>>(buckets).clear(resource), ...);
    }
};

}  // namespace oge
