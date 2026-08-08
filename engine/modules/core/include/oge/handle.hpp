#pragma once

#include <cstdint>
#include <functional>

namespace oge
{

template <auto Tag>
struct Handle
{
    uint16_t index = 0;
    uint16_t generation = 0;

    constexpr bool IsValid() const noexcept
    {
        return index != 0;
    }

    static Handle<Tag> InvalidHandle()
    {
        return Handle<Tag>{0, 0};
    }

    bool operator==(const Handle<Tag>& other) const noexcept
    {
        return index == other.index && generation == other.generation;
    }

    
};

template <typename Handle>
struct HandleHash
{
    size_t operator()(const Handle& slot) const noexcept
    {
        return (static_cast<size_t>(slot.index) << 16) | slot.generation;
    }
};

}  // namespace oge

// Specialise std::hash so that oge::Handle<Tag> works out of the box
// with std::unordered_map / std::unordered_set without an explicit
// Hash template argument.
namespace std
{
template <auto Tag>
struct hash<oge::Handle<Tag>>
{
    size_t operator()(const oge::Handle<Tag>& h) const noexcept
    {
        return oge::HandleHash<oge::Handle<Tag>>{}(h);
    }
};
}  // namespace std
