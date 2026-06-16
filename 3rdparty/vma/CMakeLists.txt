fetch_file(${CMAKE_CURRENT_SOURCE_DIR}/include/vk_mem_alloc.h https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/refs/heads/master/include/vk_mem_alloc.h)

add_library(vma STATIC
    vma.cpp
)

target_include_directories(vma PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
target_link_libraries(vma
    PRIVATE
        Vulkan::Vulkan
)
else()

target_compile_definitions(vma PRIVATE
    VMA_VULKAN_VERSION=1001000
)

target_link_libraries(vma
    PRIVATE
        vulkan
)
endif()