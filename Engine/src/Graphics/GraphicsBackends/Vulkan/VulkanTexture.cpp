#include <cassert>
#include "Vulkan.hpp"
#include "VulkanTexture.hpp"

//extern "C" {
#include "vk_mem_alloc.h"
//}

namespace OneGame::Engine::Graphics::Vulkan
{
    GPUTextureHandle VulkanBackend::CreateTexture(const TextureDesc& desc)
    {
        assert(desc.width > 0 && desc.height > 0);
        assert(desc.depth == 1);
        assert(desc.mipLevels == 1);
        
        VkImageUsageFlags usage = 0;
        if (HasFlag(desc.usage, TextureUsage::Sampled))
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (HasFlag(desc.usage, TextureUsage::Storage))
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        if (HasFlag(desc.usage, TextureUsage::ColorAttachment))
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (HasFlag(desc.usage, TextureUsage::DepthAttachment))
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if (HasFlag(desc.usage, TextureUsage::TransferSrc))
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (HasFlag(desc.usage, TextureUsage::TransferDst))
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkFormat vkFormat;
        VkImageAspectFlags aspectMask;

        GetVkFormatAndAspect(desc.format, vkFormat, aspectMask);

        VulkanTexture texture;
        CreateTextureInternal(desc.width, desc.height, desc.depth, desc.mipLevels, usage, vkFormat, aspectMask, texture);

        return m_textures.Create(texture);
    }

    void VulkanBackend::CreateTextureInternal(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        uint32_t mipLevels,
        VkImageUsageFlags vkUsage,
        VkFormat vkFormat,
        VkImageAspectFlags aspectMask,
        VulkanTexture& result)
    {
        result.width = width;
        result.height = height;
        result.mipLevels = mipLevels;
        result.format = vkFormat;
        result.aspect = aspectMask;

        // --- Image Create Info ---
        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = vkFormat;
        ici.extent = { width, height, depth };
        ici.mipLevels = mipLevels;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = vkUsage;

        // --- Allocate with VMA ---
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VK_CHECK_RESULT(vmaCreateImage(
            m_device.m_allocator,
            &ici,
            &allocInfo,
            &result.image,
            &result.allocation,
            nullptr));

        // --- Image View ---
        VkImageViewCreateInfo ivci{};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = result.image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = vkFormat;
        ivci.subresourceRange.aspectMask = aspectMask;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = mipLevels;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        VK_CHECK_RESULT(vkCreateImageView(
            m_device.device,
            &ivci,
            nullptr,
            &result.view));

        // sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        //VkPhysicalDeviceFeatures features;
        //vkGetPhysicalDeviceFeatures(m_device.physicalDevice, &features);

        //if (!features.samplerAnisotropy)
        //{
        //    samplerInfo.anisotropyEnable = VK_FALSE;
        //}
        //else
        //{
        //    VkPhysicalDeviceProperties props;
        //    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &props);
        //    float maxSupported =
        //        props.limits.maxSamplerAnisotropy;
        //    samplerInfo.anisotropyEnable = VK_TRUE;
        //    samplerInfo.maxAnisotropy = 8.0f < maxSupported ? 8.0f : maxSupported;
        //}

        samplerInfo.anisotropyEnable = VK_FALSE;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f;

        if (vkCreateSampler(
            m_device.device,
            &samplerInfo,
            nullptr,
            &result.sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create sampler");
        }
    }

	void VulkanBackend::DestroyTextureInternal(VulkanTexture& texture)
	{
        if (texture.sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_device.device, texture.sampler, nullptr);
            texture.sampler = VK_NULL_HANDLE;
        }
        if (texture.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device.device, texture.view, nullptr);
            texture.view = VK_NULL_HANDLE;
        }
        if (texture.image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(m_device.m_allocator, texture.image, texture.allocation);
            texture.image = VK_NULL_HANDLE;
        }
	}

    void VulkanBackend::DestroyTexture(GPUTextureHandle handle)
    {
		auto texture = m_textures.Get(handle);
		DestroyTextureInternal(*texture);
		m_textures.Destroy(handle);
    }
}