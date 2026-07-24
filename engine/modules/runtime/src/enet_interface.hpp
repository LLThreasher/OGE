#pragma once

#include <memory_resource>
#include <enet/enet.h>

#include "oge/runtime/net_serializer.hpp"

extern "C" {
inline std::pmr::memory_resource* oge_enet_memory;

inline void* oge_enet_alloc(size_t size)
{
    return oge_enet_memory->allocate(size);
}

inline void oge_enet_free(void* data, size_t size)
{
    oge_enet_memory->deallocate(data, size);
}

inline ENetCallbacks oge_enet_callback = {oge_enet_alloc, oge_enet_free, abort};

inline int oge_enet_initialize(std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
{
    oge_enet_memory = memory;
    return enet_initialize_with_callbacks(ENET_VERSION, &oge_enet_callback);
}

inline void oge_enet_shutdown()
{
    enet_deinitialize();
}
}
