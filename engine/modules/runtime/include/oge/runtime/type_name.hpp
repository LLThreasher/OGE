#pragma once

#include <string>

#include "oge/runtime/entt.hpp"

namespace oge::runtime
{

using oge_id_type = entt::id_type;

template <typename T>
struct TypeName
{
    static constexpr std::string Get();
};

}  // namespace oge::runtime

#define DECL_TYPE_NAME(Type, Name)         \
    template <>                            \
    struct ::oge::runtime::TypeName<Type>  \
    {                                      \
        static constexpr std::string Get() \
        {                                  \
            return Name;                   \
        }                                  \
    };
