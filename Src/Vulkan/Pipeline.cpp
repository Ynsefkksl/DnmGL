#include "DnmGL/Vulkan/Pipeline.hpp"
#include "DnmGL/Vulkan/Image.hpp"
#include "DnmGL/Vulkan/Shader.hpp"
#include "DnmGL/Vulkan/ResourceManager.hpp"
#include "DnmGL/Vulkan/ToVkFormat.hpp"

namespace DnmGL::Vulkan {
    static constexpr vk::PipelineStageFlagBits ShaderStageToPipelineStage(ShaderStageBits stage) {
        switch (stage) {
            case ShaderStageBits::eNone: return vk::PipelineStageFlagBits::eAllCommands;
            case ShaderStageBits::eVertex: return vk::PipelineStageFlagBits::eVertexShader;
            case ShaderStageBits::eFragment: return vk::PipelineStageFlagBits::eFragmentShader;
            case ShaderStageBits::eCompute: return vk::PipelineStageFlagBits::eComputeShader;
        }
    }

    static constexpr vk::AccessFlags GetAccessFlagsForEntryPoint(const EntryPointInfo &stage) {
        vk::AccessFlags out;
        if (stage.HasReadonlyResource())
            out |= vk::AccessFlagBits::eShaderRead;
        if (stage.HasWritableResource())
            out |= vk::AccessFlagBits::eShaderWrite;
        if (stage.HasUniformResource())
            out |= vk::AccessFlagBits::eUniformRead;
        return out;
    }

    GraphicsPipelineBase::GraphicsPipelineBase(Vulkan::Context& ctx, const DnmGL::GraphicsPipelineDesc& desc) noexcept
        : DnmGL::GraphicsPipeline(ctx, desc) {
        const auto *typed_vertex_shader = static_cast<const Vulkan::Shader *>(m_desc.vertex_shader);
        const auto *typed_fragment_shader = static_cast<const Vulkan::Shader *>(m_desc.fragment_shader);
        const auto *vertex_entry_point = typed_vertex_shader->GetEntryPoint(m_desc.vertex_entry_point);
        const auto *frag_entry_point = typed_fragment_shader->GetEntryPoint(m_desc.fragment_entry_point);
        {
            m_pipeline_stage_flags = 
                ShaderStageToPipelineStage(vertex_entry_point->shader_stage)
                | ShaderStageToPipelineStage(frag_entry_point->shader_stage)
                ;

            
            m_access_flags |= GetAccessFlagsForEntryPoint(*vertex_entry_point);
            m_access_flags |= GetAccessFlagsForEntryPoint(*frag_entry_point);
        }

        const auto *typed_resource_manager = static_cast<const Vulkan::ResourceManager *>(m_desc.resource_manager);
        const auto device = VulkanContext->GetDevice();

        {
            std::vector<vk::PushConstantRange> push_constants{};
            
            vk::DescriptorSetLayout dst_set_layouts[4];
            const EntryPointInfo* entry_points[2] = {vertex_entry_point, frag_entry_point};

            typed_resource_manager->FillDescriptorSets(m_dst_sets, entry_points);
            typed_resource_manager->FillDescriptorSetLayouts(dst_set_layouts, entry_points);

            vk::PipelineLayoutCreateInfo create_info{};
            create_info.setSetLayouts(dst_set_layouts)
                        ;

            m_pipeline_layout = device.createPipelineLayout(create_info);
        }

        m_sample_count = VulkanContext->GetSampleCount(m_desc.msaa, HasStencilAttachment());
    }

    GraphicsPipelineDynamicRendering::GraphicsPipelineDynamicRendering(Vulkan::Context& ctx, const DnmGL::GraphicsPipelineDesc& desc) noexcept 
        : Vulkan::GraphicsPipelineBase(ctx, desc) {
        m_pipeline = CreatePipeline(VK_NULL_HANDLE);
    }

    GraphicsPipelineDefaultVk::GraphicsPipelineDefaultVk(Vulkan::Context& ctx, const DnmGL::GraphicsPipelineDesc& desc) noexcept
        : GraphicsPipelineBase(ctx, desc) {}

    vk::RenderPass GraphicsPipelineDefaultVk::CreateRenderpass(AttachmentOps attachment_ops, bool presenting) noexcept {
        const auto device = VulkanContext->GetDevice();

        std::vector<vk::AttachmentDescription> attachment_descs{};
        std::vector<vk::AttachmentReference> color_references{};
        std::vector<vk::AttachmentReference> resolve_references{};

        for (const auto i : Counter(ColorAttachmentCount())) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                presenting ? VulkanContext->GetSwapchainProperties().format : ToVkFormat(m_desc.color_attachment_formats[i]),
                vk::SampleCountFlagBits::e1,
                ToVk(attachment_ops.color_load[i]),
                ToVk(attachment_ops.color_store[i]), 
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                attachment_ops.color_load[i] == AttachmentLoadOp::eLoad ? vk::ImageLayout::eColorAttachmentOptimal : vk::ImageLayout::eUndefined,
                presenting ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eColorAttachmentOptimal
            );

            if (HasMsaa()) {
                resolve_references.emplace_back(
                    i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }
            else {
                color_references.emplace_back(
                    i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }
        }

        if (HasMsaa())
            for (const auto i : Counter(ColorAttachmentCount())) {
                attachment_descs.emplace_back(
                    vk::AttachmentDescriptionFlags{},
                    presenting ? VulkanContext->GetSwapchainProperties().format : ToVkFormat(m_desc.color_attachment_formats[i]),
                    GetSampleCount(),
                    vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::AttachmentLoadOp::eDontCare,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eColorAttachmentOptimal
                );

                color_references.emplace_back(
                    ColorAttachmentCount() + i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }

        vk::AttachmentReference depth_stencil_reference{};
        depth_stencil_reference.setAttachment(color_references.size() * (HasMsaa() + 1))
                                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                ;

        if (HasDepthAttachment() && HasStencilAttachment()) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                ToVkFormat(m_desc.depth_stencil_format),
                GetSampleCount(),
                ToVk(attachment_ops.depth_load),
                ToVk(attachment_ops.depth_store),
                ToVk(attachment_ops.stencil_load),
                ToVk(attachment_ops.stencil_store),
                (attachment_ops.depth_load == AttachmentLoadOp::eLoad) || (attachment_ops.stencil_load == AttachmentLoadOp::eLoad) ? 
                                    vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            );
        }
        else if (HasDepthAttachment()) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                ToVkFormat(m_desc.depth_stencil_format),
                GetSampleCount(),
                ToVk(attachment_ops.depth_load),
                ToVk(attachment_ops.depth_store),
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                attachment_ops.depth_load == AttachmentLoadOp::eLoad ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            );
        }

        vk::SubpassDescription subpass_desc{};
        subpass_desc.setPDepthStencilAttachment(
                        (HasDepthAttachment() || HasStencilAttachment()) 
                        ? &depth_stencil_reference : nullptr)
                    .setResolveAttachments(resolve_references)
                    .setColorAttachments(color_references)
                    ;

        vk::SubpassDependency subpass_dependency{};
        subpass_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                            .setSrcAccessMask({})
                            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                            .setDstSubpass(0)
                            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                            ;

        if (HasDepthAttachment() || HasStencilAttachment()) {
            subpass_dependency.dstAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            subpass_dependency.dstStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
            subpass_dependency.srcStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
        }

        vk::RenderPassCreateInfo create_info{};
        create_info.setSubpasses({subpass_desc})
                    .setAttachments({attachment_descs})
                    .setDependencies({subpass_dependency});

        return device.createRenderPass(create_info, nullptr);
    }

    vk::Pipeline GraphicsPipelineBase::CreatePipeline(vk::RenderPass renderpass) noexcept {
        const auto *typed_vertex_shader = static_cast<const Vulkan::Shader *>(m_desc.vertex_shader);
        const auto *typed_fragment_shader = static_cast<const Vulkan::Shader *>(m_desc.fragment_shader);

        const auto device = VulkanContext->GetDevice();

        const auto swapchain_properties = VulkanContext->GetSwapchainProperties();

        std::vector<vk::VertexInputAttributeDescription> vertex_attrib_desc{};
        vk::VertexInputBindingDescription vertex_binding_desc{};

        {
            uint32_t location{};
            uint32_t offset{};
            for (const auto& binding_format : m_desc.vertex_binding_formats) {
                const auto size = GetFormatSize(binding_format);
                DnmGLAssert(size, "unsupported vertex binding format; location: {}", location)
                vertex_attrib_desc.emplace_back(
                    location++,
                    0,
                    ToVkFormat(binding_format),
                    offset
                );
                offset += size;
            }

            vertex_binding_desc
                .setInputRate(vk::VertexInputRate::eVertex)
                .setBinding(0)
                .setStride(offset);
        }

        vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
        vertex_input_state_create_info.setVertexAttributeDescriptions(vertex_attrib_desc)
                                    .setPVertexBindingDescriptions(&vertex_binding_desc)
                                    .setVertexBindingDescriptionCount(vertex_attrib_desc.size() ? 1 : 0)
                                    ;

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{};
        input_assembly_info.setTopology(static_cast<vk::PrimitiveTopology>(m_desc.topology));
    
        vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]{};
        shader_stage_create_info[0].setStage(vk::ShaderStageFlagBits::eVertex)
                                    .setModule(typed_vertex_shader->GetShaderModule())
                                    .setPName(m_desc.vertex_entry_point.c_str())
                                    ;

        shader_stage_create_info[1].setStage(vk::ShaderStageFlagBits::eFragment)
                                    .setModule(typed_fragment_shader->GetShaderModule())
                                    .setPName(m_desc.fragment_entry_point.c_str())
                                    ;

        vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
        viewport_state_create_info
            .setScissorCount(1).setViewportCount(1);

        vk::PipelineRasterizationStateCreateInfo rester_info{};
        rester_info.setPolygonMode(static_cast<vk::PolygonMode>(m_desc.polygone_mode))
                    .setLineWidth(1.f)
                    .setCullMode(static_cast<vk::CullModeFlagBits>(m_desc.cull_mode))
                    .setFrontFace(static_cast<vk::FrontFace>(m_desc.front_face))
                    ;

        vk::PipelineMultisampleStateCreateInfo multisample_Info{};
        multisample_Info.setSampleShadingEnable(vk::False)
                        .setRasterizationSamples(m_sample_count)
                        .setMinSampleShading(1.f)
                        ;

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{};
        depth_stencil_info.setDepthTestEnable(m_desc.depth_test)
                            .setDepthWriteEnable(m_desc.depth_write)
                            .setDepthCompareOp(static_cast<vk::CompareOp>(m_desc.depth_test_compare_op))
                            .setStencilTestEnable(m_desc.stencil_test)
                            ;

        std::vector<vk::PipelineColorBlendAttachmentState> blend_state{};
        for (auto i : Counter(m_desc.color_attachment_formats.size()))
                blend_state.emplace_back(vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(m_desc.color_blend)
                    .setColorWriteMask(
                        vk::ColorComponentFlagBits::eA | 
                        vk::ColorComponentFlagBits::eR | 
                        vk::ColorComponentFlagBits::eB | 
                        vk::ColorComponentFlagBits::eG)
                    .setAlphaBlendOp(vk::BlendOp::eAdd)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                );

        vk::PipelineColorBlendStateCreateInfo color_blend_info{};
        color_blend_info.setLogicOpEnable(vk::False)
                        .setAttachments(blend_state);

        constexpr vk::DynamicState dynamic_states[2] {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{};
        dynamic_state_create_info.setDynamicStates(dynamic_states);

        vk::PipelineRenderingCreateInfoKHR rendering_info{};

        vk::GraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.setLayout(m_pipeline_layout)
                        .setStages(shader_stage_create_info)
                        .setPViewportState(&viewport_state_create_info)
                        .setPRasterizationState(&rester_info)
                        .setPMultisampleState(&multisample_Info)
                        .setPDepthStencilState(&depth_stencil_info)
                        .setPColorBlendState(&color_blend_info)
                        .setPInputAssemblyState(&input_assembly_info)
                        .setPDynamicState(&dynamic_state_create_info)
                        .setPVertexInputState(&vertex_input_state_create_info)
                        .setSubpass(0)
                        ;

        std::vector<vk::Format> color_formats{};
        for (const auto format : m_desc.color_attachment_formats) {
            color_formats.emplace_back(ToVkFormat(format));
        }

        if (renderpass) {
            pipeline_info.setRenderPass(renderpass);
        }
        else {
            rendering_info.setColorAttachmentFormats(color_formats);
            if (HasDepthAttachment()) rendering_info.setDepthAttachmentFormat(ToVkFormat(m_desc.depth_stencil_format));
            if (HasStencilAttachment()) rendering_info.setStencilAttachmentFormat(ToVkFormat(m_desc.depth_stencil_format));

            pipeline_info.setPNext(&rendering_info);
        }

        return device.createGraphicsPipeline(VulkanContext->GetPipelineCache(), pipeline_info).value;
    }

    vk::RenderPass GraphicsPipelineDefaultVk::GetRenderpass(uint32_t packed_attachment_ops) noexcept {
        const auto it = m_renderpasses.find(packed_attachment_ops);
        if (it == m_renderpasses.end()) return VK_NULL_HANDLE;
        return it->second;
    }

    vk::Pipeline GraphicsPipelineDefaultVk::GetPipeline(const vk::RenderPass renderpass) noexcept {
        const auto it = m_pipelines.find(renderpass);
        if (it == m_pipelines.end()) return VK_NULL_HANDLE;
        return it->second;
    }

    std::pair<vk::RenderPass, vk::Pipeline> GraphicsPipelineDefaultVk::GetOrCreateAttachmentOpVariant(AttachmentOps attachment_ops, bool presenting) {
        const auto packed_attachment_ops = attachment_ops.GetPacked(presenting);
        auto it = m_renderpasses.try_emplace(packed_attachment_ops, vk::RenderPass{});
        if (!it.second) {
            return {it.first->second, m_pipelines.at(it.first->second)};
        }

        const auto vk_renderpass = CreateRenderpass(attachment_ops, presenting);
        const auto vk_pipeline = CreatePipeline(vk_renderpass);

        m_pipelines.emplace(vk_renderpass, vk_pipeline);
        it.first->second = vk_renderpass;

        return {vk_renderpass, m_pipelines.at(vk_renderpass)};
    }

    ComputePipeline::ComputePipeline(Vulkan::Context& ctx, const DnmGL::ComputePipelineDesc& desc) noexcept
        : DnmGL::ComputePipeline(ctx, desc) {

        const auto *typed_shader = static_cast<const Vulkan::Shader *>(m_desc.shader);
        const auto *shader_entry_point = static_cast<const Vulkan::Shader *>(m_desc.shader)->GetEntryPoint(m_desc.shader_entry_point);
        {
            m_pipeline_stage_flags = 
                ShaderStageToPipelineStage(shader_entry_point->shader_stage);

            m_access_flags |= GetAccessFlagsForEntryPoint(*shader_entry_point);
        }

        const auto *typed_resource_manager = static_cast<const Vulkan::ResourceManager *>(m_desc.resource_manager);
        const auto device = VulkanContext->GetDevice();

        vk::PipelineShaderStageCreateInfo stage_info{};
        stage_info.setStage(vk::ShaderStageFlagBits::eCompute)
                    .setModule(typed_shader->GetShaderModule())
                    .setPName(m_desc.shader_entry_point.c_str())
                    ;

        vk::ComputePipelineCreateInfo pipeline_info{};
        pipeline_info.setLayout(nullptr)
                    .setStage(stage_info)
                    .setLayout(m_pipeline_layout)
                    ;

        m_pipeline = device.createComputePipeline(nullptr, pipeline_info).value;
    }
}