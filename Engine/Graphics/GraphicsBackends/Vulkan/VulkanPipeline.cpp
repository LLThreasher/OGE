#include "../Vulkan.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanBindingGroups.hpp"
#include "VulkanFrameBuffer.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
    VkShaderModule VulkanBackend::CreateShaderModule(
        const std::vector<uint8_t>& code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule module;
        if (vkCreateShaderModule(
            m_device.device,
            &createInfo,
            nullptr,
            &module) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module");
        }

        return module;
    }

    GPUPipelineHandle VulkanBackend::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
    {
        VulkanPipeline pipeline{};

        //
        // --- Shader stages
        //
        VkShaderModule vertModule = CreateShaderModule(desc.vertexShader);
        VkShaderModule fragModule = CreateShaderModule(desc.fragmentShader);

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] =
        {
            vertStage,
            fragStage
        };

        //
        // --- Vertex Input
        //
        std::vector<VkVertexInputAttributeDescription> attributes;

        uint32_t offset = 0;
        uint32_t location = 0;

        for (auto format : desc.vertexLayout)
        {
            auto vkFormat = GetFormatInfo(format);

            VkVertexInputAttributeDescription attr{};
            attr.location = location++;
            attr.binding = 0;
            attr.format = vkFormat.vkFormat;
            attr.offset = offset;

            offset += vkFormat.size;

            attributes.push_back(attr);
        }

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = offset;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        //
        // --- Input Assembly
        //
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        //
        // --- Viewport & Scissor (dynamic)
        //
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        std::array<VkDynamicState, 2> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount =
            static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        //
        // --- Rasterizer
        //
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = ToVkCullMode(desc.cullMode);
        rasterizer.frontFace = ToVkFrontFace(desc.frontFace);
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;

        //
        // --- Multisampling
        //
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        //
        // --- Depth Stencil
        //
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.depthTest;
        depthStencil.depthWriteEnable = desc.writeDepth;
        depthStencil.depthCompareOp = ToVkCompareOp(desc.depthCompareOp);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        //
        // --- Color Blend
        //
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = desc.blending;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        //
        // --- Pipeline Layout
        //
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount =
            static_cast<uint32_t>(desc.bindingGroupLayouts.size());
        std::vector<VkDescriptorSetLayout> vkLayouts(desc.bindingGroupLayouts.size());
        for (int i = 0; i < desc.bindingGroupLayouts.size(); i++)
        {
            vkLayouts[i] = m_bindingGroupLayouts.Get(desc.bindingGroupLayouts[i]).layout;
        }
        layoutInfo.pSetLayouts = vkLayouts.data();

        if (vkCreatePipelineLayout(
            m_device.device,
            &layoutInfo,
            nullptr,
            &pipeline.layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        //
        // --- Graphics Pipeline
        //
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType =
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount =
            static_cast<uint32_t>(2);
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipeline.layout;
        pipelineInfo.renderPass = m_renderPasses.Get(m_swapchain.renderPass).handle;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(
            m_device.device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline.pipeline) != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(m_device.device, pipeline.layout, nullptr);
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        pipeline.bindingPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        return m_pipelines.Create(pipeline);
    }

    GPUPipelineHandle VulkanBackend::CreateComputePipeline(const ComputePipelineDesc& desc)
    {

        VulkanPipeline pipeline{};

        //
        // Layout
        //
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount =
            static_cast<uint32_t>(desc.bindingGroupLayouts.size());
        std::vector<VkDescriptorSetLayout> vkLayouts(desc.bindingGroupLayouts.size());
        for (int i = 0; i < desc.bindingGroupLayouts.size(); i++)
        {
            vkLayouts[i] = m_bindingGroupLayouts.Get(desc.bindingGroupLayouts[i]).layout;
        }
        layoutInfo.pSetLayouts = vkLayouts.data();

        if (vkCreatePipelineLayout(
            m_device.device,
            &layoutInfo,
            nullptr,
            &pipeline.layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create compute layout");
        }

        //
        // Shader stage
        //
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = CreateShaderModule(desc.shader);
        stage.pName = "main";

        //
        // Compute pipeline
        //
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType =
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stage;
        pipelineInfo.layout = pipeline.layout;

        if (vkCreateComputePipelines(
            m_device.device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline.pipeline) != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(m_device.device, pipeline.layout, nullptr);
            throw std::runtime_error("Failed to create compute pipeline");
        }

        pipeline.bindingPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        return m_pipelines.Create(pipeline);
    }

    void VulkanBackend::DestroyPipeline(GPUPipelineHandle handle)
    {
        VulkanPipeline& pipeline = m_pipelines.Get(handle);

        if (pipeline.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device.device, pipeline.pipeline, nullptr);
        }

        if (pipeline.layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_device.device, pipeline.layout, nullptr);
        }

        pipeline = {};
        m_pipelines.Destroy(handle);
    }

}
