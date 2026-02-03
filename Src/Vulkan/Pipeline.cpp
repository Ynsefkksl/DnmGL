#include "DnmGL/Vulkan/Pipeline.hpp"
#include "DnmGL/Vulkan/Image.hpp"
#include "DnmGL/Vulkan/Buffer.hpp"
#include "DnmGL/Vulkan/Sampler.hpp"
#include "DnmGL/Vulkan/ToVkFormat.hpp"

//
//WARNING: SHIT CODE
//

namespace DnmGL::Vulkan {
    static constexpr vk::ShaderStageFlagBits GetVkShaderStage(ShaderStageBits stage) {
        switch (stage) {
            // ShaderStageBits::eNone handled in Shader constructer
            case ShaderStageBits::eNone: std::unreachable();
            case ShaderStageBits::eVertex: return vk::ShaderStageFlagBits::eVertex;
            case ShaderStageBits::eFragment: return vk::ShaderStageFlagBits::eFragment;
            case ShaderStageBits::eCompute: return vk::ShaderStageFlagBits::eCompute;
        }
    }

    static constexpr vk::DescriptorType GetVkDescriptorType(ResourceType type) {
        switch (type) {
            // ResourceType::eNone handled in Shader constructer
            case ResourceType::eNone: std::unreachable();
            case ResourceType::eReadonlyBuffer: return vk::DescriptorType::eStorageBuffer;
            case ResourceType::eReadonlyImage: return vk::DescriptorType::eSampledImage;
            case ResourceType::eWritableBuffer: return vk::DescriptorType::eStorageBuffer;
            case ResourceType::eWritableImage: return vk::DescriptorType::eStorageImage;
            case ResourceType::eUniformBuffer: return vk::DescriptorType::eUniformBuffer;
            case ResourceType::eSampler: return vk::DescriptorType::eSampler;
          break;
        }
    }

    static auto GetUpdateResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint32_t array_index, vk::DescriptorSet set) {
        const auto *typed_buffer
            = static_cast<const Vulkan::Buffer *>(buffer_desc.buffer);

        return Context::UpdateBufferResource(
            typed_buffer->GetBuffer(),
            GetVkDescriptorType(resource.type),
            buffer_desc.first_element * typed_buffer->GetDesc().element_size,
            buffer_desc.element_count * typed_buffer->GetDesc().element_size,
            set,
            resource.spirv_index,
            array_index
        );
    }

    static auto GetUpdateResource(const Resource &resource, const ImageResourceDesc &image_desc, uint32_t array_index, vk::DescriptorSet set) {
        auto *typed_image
            = static_cast<Vulkan::Image *>(image_desc.image);

        return Context::UpdateImageResource(
            typed_image->CreateGetImageView(image_desc.subresource),
            typed_image->GetIdealImageLayout(),
            GetVkDescriptorType(resource.type),
            set,
            resource.spirv_index,
            array_index
        );
    }

    static auto GetUpdateResource(const Resource &resource, const DnmGL::Sampler *sampler, uint32_t array_index, vk::DescriptorSet set) {
        return Context::UpdateSamplerResource(
            static_cast<const Vulkan::Sampler *>(sampler)->GetSampler(),
            set,
            resource.spirv_index,
            array_index
        );
    }

    static std::pair<vk::PipelineLayout, vk::DescriptorSetLayout> GetPipelineLayoutFromShaderData(Vulkan::Context &context, const ShaderReflection &shader_reflection, bool is_compute) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(shader_reflection.resources.size());
        for (const auto &[_, res] : shader_reflection.resources)
            bindings.emplace_back(
                res.spirv_index,
                GetVkDescriptorType(res.type),
                res.resource_count,
                is_compute ? vk::ShaderStageFlagBits::eCompute : (vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            );

        vk::DescriptorSetLayoutCreateFlagBits s;
        vk::DescriptorSetLayout dst_set_layout = context.GetDevice().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(bindings)
                .setFlags(
                    context.GetSupportedFeatures().descriptor_update_after_bind
                        ? vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPoolEXT : vk::DescriptorSetLayoutCreateFlagBits{}
                ));

        return {
            context.GetDevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{}.setSetLayouts(dst_set_layout)),
            dst_set_layout
        };
    }

    GraphicsPipelineBase::GraphicsPipelineBase(Vulkan::Context &ctx, const DnmGL::GraphicsPipelineDesc &desc) noexcept
        : DnmGL::GraphicsPipeline(ctx, desc) {
        const auto device = VulkanContext->GetDevice();

        {
            const auto [pipeline_layout, descriptor_layout] = GetPipelineLayoutFromShaderData(ctx, m_shader_data->reflection, false);
    
            m_dst_set_layout = descriptor_layout;
            m_pipeline_layout = pipeline_layout;
            m_dst_set = VulkanContext->CreateDescriptorSet(m_dst_set_layout);
        }

        m_shader_module = device.createShaderModule(vk::ShaderModuleCreateInfo(
                vk::ShaderModuleCreateFlags{},
                m_shader_data->spirv_code.size(),
                reinterpret_cast<const uint32_t *>(m_shader_data->spirv_code.data())
            )
        );
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
                presenting ? VulkanContext->GetSwapchainProperties().format : ToVkFormat(m_resterizer_desc.color_attachment_formats[i]),
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
                    presenting ? VulkanContext->GetSwapchainProperties().format : ToVkFormat(m_resterizer_desc.color_attachment_formats[i]),
                    VulkanContext->GetSampleCount(m_resterizer_desc.msaa),
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
                ToVkFormat(m_depth_stencil_desc.depth_stencil_format),
                VulkanContext->GetSampleCount(m_resterizer_desc.msaa),
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
                ToVkFormat(m_depth_stencil_desc.depth_stencil_format),
                VulkanContext->GetSampleCount(m_resterizer_desc.msaa),
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
        const auto device = VulkanContext->GetDevice();

        const auto swapchain_properties = VulkanContext->GetSwapchainProperties();

        std::vector<vk::VertexInputAttributeDescription> vertex_attrib_desc{};
        vk::VertexInputBindingDescription vertex_binding_desc{};

        {
            uint32_t location{};
            uint32_t offset{};
            for (const auto& [binding_format, offseta] : m_input_assambly_desc.vertex_bindings) {
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
        input_assembly_info.setTopology(static_cast<vk::PrimitiveTopology>(m_input_assambly_desc.topology));

        vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]{};
        shader_stage_create_info[0].setStage(vk::ShaderStageFlagBits::eVertex)
                                    .setModule(m_shader_module);

        shader_stage_create_info[1].setStage(vk::ShaderStageFlagBits::eFragment)
                                    .setModule(m_shader_module);
        for (const auto &[_, entry_point] : m_shader_data->reflection.entry_points) {
            if (entry_point.shader_stage == ShaderStageBits::eVertex)
                shader_stage_create_info[0].setPName(entry_point.name.c_str());

            if (entry_point.shader_stage == ShaderStageBits::eFragment)
                shader_stage_create_info[1].setPName(entry_point.name.c_str());
        }

        vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
        viewport_state_create_info
            .setScissorCount(1).setViewportCount(1);

        vk::PipelineRasterizationStateCreateInfo rester_info{};
        rester_info.setPolygonMode(static_cast<vk::PolygonMode>(m_resterizer_desc.polygone_mode))
                    .setLineWidth(1.f)
                    .setCullMode(static_cast<vk::CullModeFlagBits>(m_resterizer_desc.cull_mode))
                    .setFrontFace(static_cast<vk::FrontFace>(m_resterizer_desc.front_face))
                    ;

        vk::PipelineMultisampleStateCreateInfo multisample_Info{};
        multisample_Info.setSampleShadingEnable(vk::False)
                        .setRasterizationSamples(VulkanContext->GetSampleCount(m_resterizer_desc.msaa))
                        .setMinSampleShading(1.f)
                        ;

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{};
        depth_stencil_info.setDepthTestEnable(m_depth_stencil_desc.depth_test)
                            .setDepthWriteEnable(m_depth_stencil_desc.depth_write)
                            .setDepthCompareOp(static_cast<vk::CompareOp>(m_depth_stencil_desc.depth_test_compare_op))
                            .setStencilTestEnable(m_depth_stencil_desc.stencil_test)
                            .setBack(vk::StencilOpState{})
                            .setFront(vk::StencilOpState{})
                            ;

        std::vector<vk::PipelineColorBlendAttachmentState> blend_state{};
        for (auto i : Counter(m_resterizer_desc.color_attachment_formats.size()))
                blend_state.emplace_back(vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(m_resterizer_desc.color_blend)
                    .setColorWriteMask(
                        vk::ColorComponentFlagBits::eA | 
                        vk::ColorComponentFlagBits::eR | 
                        vk::ColorComponentFlagBits::eB | 
                        vk::ColorComponentFlagBits::eG)
                    .setAlphaBlendOp(vk::BlendOp::eAdd)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
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
        for (const auto format : m_resterizer_desc.color_attachment_formats) {
            color_formats.emplace_back(ToVkFormat(format));
        }

        if (renderpass) {
            pipeline_info.setRenderPass(renderpass);
        }
        else {
            rendering_info.setColorAttachmentFormats(color_formats);
            if (HasDepthAttachment()) rendering_info.setDepthAttachmentFormat(ToVkFormat(m_depth_stencil_desc.depth_stencil_format));
            if (HasStencilAttachment()) rendering_info.setStencilAttachmentFormat(ToVkFormat(m_depth_stencil_desc.depth_stencil_format));

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

    ComputePipeline::ComputePipeline(Vulkan::Context& ctx, std::string_view shader) noexcept
        : DnmGL::ComputePipeline(ctx, shader) {
        const auto device = VulkanContext->GetDevice();

        {
            const auto [pipeline_layout, descriptor_layout] = GetPipelineLayoutFromShaderData(ctx, m_shader_data->reflection, true);
    
            m_dst_set_layout = descriptor_layout;
            m_pipeline_layout = pipeline_layout;
            m_dst_set = VulkanContext->CreateDescriptorSet(m_dst_set_layout);
        }

        m_shader_module = device.createShaderModule(vk::ShaderModuleCreateInfo(
                vk::ShaderModuleCreateFlags{},
                m_shader_data->spirv_code.size() / 4,
                reinterpret_cast<const uint32_t *>(m_shader_data->spirv_code.data())
            )
        );

        vk::PipelineShaderStageCreateInfo stage_info{};
        stage_info.setStage(vk::ShaderStageFlagBits::eCompute)
                    .setModule(m_shader_module)
                    ;

        for (const auto &[stage_name, _] : m_shader_data->reflection.entry_points) {
            stage_info.setPName(stage_name.c_str());

            vk::ComputePipelineCreateInfo pipeline_info{};
            pipeline_info.setStage(stage_info)
                        .setLayout(m_pipeline_layout)
                        ;
    
            m_pipelines.emplace(
                stage_name, 
                device.createComputePipeline(nullptr, pipeline_info).value);
        }
    }
    
    void ComputePipeline::ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, buffer_desc, array_index, m_dst_set));
    }

    void ComputePipeline::ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, image_desc, array_index, m_dst_set));
    }

    void ComputePipeline::ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, sampler, array_index, m_dst_set));
    }
    
    void GraphicsPipelineBase::ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, buffer_desc, array_index, m_dst_set));
    }

    void GraphicsPipelineBase::ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, image_desc, array_index, m_dst_set));
    }

    void GraphicsPipelineBase::ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint32_t array_index) {
        VulkanContext->UpdateDescriptor(GetUpdateResource(resource, sampler, array_index, m_dst_set));
    }
}