#pragma once

#include "../Vulkan.hpp"
#include "VulkanFrameBuffer.hpp"
#include "VulkanTexture.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
    GPURenderPassHandle VulkanBackend::CreateRenderPass(
        const RenderPassDesc& desc)
    {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;

        VkAttachmentReference depthRef{};
        bool hasDepth = desc.hasDepth;

        // --------------------------------------------------
        // Color Attachments
        // --------------------------------------------------

        for (size_t i = 0; i < desc.colors.size(); ++i)
        {
            const auto& color = desc.colors[i];

            VkAttachmentDescription att{};
            att.format = ToVkFormat(color.format);
            att.samples = VK_SAMPLE_COUNT_1_BIT;
            att.loadOp = ToVkLoadOp(color.loadOp);
            att.storeOp = ToVkStoreOp(color.storeOp);
            att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (color.isSwapchain)
                att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            else
                att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachments.push_back(att);

            VkAttachmentReference ref{};
            ref.attachment = static_cast<uint32_t>(i);
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorRefs.push_back(ref);
        }

        // --------------------------------------------------
        // Depth Attachment
        // --------------------------------------------------

        if (hasDepth)
        {
            const auto& depth = desc.depth;

            VkAttachmentDescription att{};
            att.format = ToVkFormat(depth.format);
            att.samples = VK_SAMPLE_COUNT_1_BIT;
            att.loadOp = ToVkLoadOp(depth.loadOp);
            att.storeOp = ToVkStoreOp(depth.storeOp);
            att.stencilLoadOp = att.loadOp;
            att.stencilStoreOp = att.storeOp;

            att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            att.finalLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            attachments.push_back(att);

            depthRef.attachment =
                static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // --------------------------------------------------
        // Subpass
        // --------------------------------------------------

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount =
            static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();

        if (hasDepth)
            subpass.pDepthStencilAttachment = &depthRef;

        // --------------------------------------------------
        // Dependency (more correct)
        // --------------------------------------------------

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        dependency.srcAccessMask = 0;

        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        if (hasDepth)
            dependency.dstAccessMask |=
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // --------------------------------------------------
        // Create
        // --------------------------------------------------

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        VkRenderPass renderPass;

        if (vkCreateRenderPass(
            m_device.device,
            &info,
            nullptr,
            &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass");
        }

        VulkanRenderPass vkPass{};
        vkPass.handle = renderPass;
        vkPass.desc = desc;

        return m_renderPasses.Create(vkPass);
    }

    void VulkanBackend::DestroyRenderPass(GPURenderPassHandle handle)
    {
        auto& pass = m_renderPasses.Get(handle);

        if (pass.handle != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_device.device, pass.handle, nullptr);
            pass.handle = VK_NULL_HANDLE;
        }
    }

    GPUFrameBufferHandle VulkanBackend::CreateFrameBuffer(
        GPURenderPassHandle passHandle,
        const FrameBufferDesc& desc)
    {
        auto& pass = m_renderPasses.Get(passHandle);

        std::vector<VkImageView> attachments;

        uint32_t width = 0;
        uint32_t height = 0;

        // --------------------------------------------------
        // Validate attachment count
        // --------------------------------------------------

        size_t expectedAttachmentCount =
            pass.desc.colors.size() +
            (pass.desc.hasDepth ? 1 : 0);

        size_t providedAttachmentCount =
            desc.colors.size() +
            (desc.hasDepth ? 1 : 0);

        if (expectedAttachmentCount != providedAttachmentCount)
        {
            throw std::runtime_error(
                "Framebuffer attachment count does not match render pass");
        }

        // --------------------------------------------------
        // Color attachments
        // --------------------------------------------------

        for (size_t i = 0; i < desc.colors.size(); ++i)
        {
            auto texHandle = desc.colors[i];
            VulkanTexture& tex = m_textures.Get(texHandle);

            // Validate format
            if (ToVkFormat(pass.desc.colors[i].format) != tex.format)
            {
                throw std::runtime_error(
                    "Framebuffer color format mismatch with render pass");
            }

            if (i == 0)
            {
                width = tex.width;
                height = tex.height;
            }
            else
            {
                if (tex.width != width || tex.height != height)
                {
                    throw std::runtime_error(
                        "Framebuffer color attachment size mismatch");
                }
            }

            attachments.push_back(tex.view);
        }

        // --------------------------------------------------
        // Depth attachment
        // --------------------------------------------------

        if (desc.hasDepth)
        {
            VulkanTexture& tex = m_textures.Get(desc.depth);

            if (!pass.desc.hasDepth)
            {
                throw std::runtime_error(
                    "Framebuffer has depth but render pass does not");
            }

            if (ToVkFormat(pass.desc.depth.format) != tex.format)
            {
                throw std::runtime_error(
                    "Framebuffer depth format mismatch");
            }

            if (tex.width != width || tex.height != height)
            {
                throw std::runtime_error(
                    "Framebuffer depth size mismatch");
            }

            attachments.push_back(tex.view);
        }

        // --------------------------------------------------
        // Create Vulkan framebuffer
        // --------------------------------------------------

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = pass.handle;
        info.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.width = width;
        info.height = height;
        info.layers = 1;

        VkFramebuffer framebuffer;

        if (vkCreateFramebuffer(
            m_device.device,
            &info,
            nullptr,
            &framebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }

        VulkanFrameBuffer vkFB{};
        vkFB.handle = framebuffer;
        vkFB.renderPass = pass.handle;
        vkFB.width = width;
        vkFB.height = height;

        return m_frameBuffers.Create(vkFB);
    }

    void VulkanBackend::DestroyFrameBuffer(
        GPUFrameBufferHandle handle)
    {
        auto& fb = m_frameBuffers.Get(handle);

        if (fb.handle != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_device.device, fb.handle, nullptr);
            fb.handle = VK_NULL_HANDLE;
        }
    }
}
