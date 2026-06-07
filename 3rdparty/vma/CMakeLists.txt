fetch_file(${CMAKE_CURRENT_SOURCE_DIR}/include/vk_mem_alloc.h https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/refs/heads/master/include/vk_mem_alloc.h)

add_library(vma STATIC
    vma.cpp
)

target_include_directories(vma PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vma
    PRIVATE
        Vulkan::Vulkan
)