#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    struct PipelineResourceAccess {
        vk::PipelineStageFlags stage_flags;
        vk::AccessFlags access_flags;
    };

    class GraphicsPipelineBase : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipelineBase(Vulkan::Context &context, const DnmGL::GraphicsPipelineDesc &desc) noexcept;

        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) override;

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] auto GetDstSet() const { return m_dst_set; }

        [[nodiscard]] PipelineResourceAccess GetResourceAccess() const noexcept { return m_resource_access; }
    protected:
        vk::Pipeline CreatePipeline(vk::RenderPass renderpass) noexcept;

        vk::DescriptorSetLayout m_dst_set_layout;
        vk::DescriptorSet m_dst_set;
        vk::PipelineLayout m_pipeline_layout;
        vk::ShaderModule m_shader_module;

        PipelineResourceAccess m_resource_access;
    };

    class GraphicsPipelineDefaultVk final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDefaultVk(Vulkan::Context &ctx, const DnmGL::GraphicsPipelineDesc &desc) noexcept;
        ~GraphicsPipelineDefaultVk() noexcept override;

        std::pair<vk::RenderPass, vk::Pipeline> GetOrCreateAttachmentOpVariant(AttachmentOps attachment_ops, bool presenting);
        vk::RenderPass GetRenderpass(uint32_t attachment_ops) noexcept;
        vk::Pipeline GetPipeline(vk::RenderPass render_pass) noexcept;
    private:
        vk::RenderPass CreateRenderpass(const AttachmentOps& attachment_ops, bool presenting) const noexcept;

        std::unordered_map<VkRenderPass, vk::Pipeline> m_pipelines;
        std::unordered_map<uint32_t, vk::RenderPass> m_renderpasses;
    };

    class GraphicsPipelineDynamicRendering final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDynamicRendering(Vulkan::Context &ctx, const DnmGL::GraphicsPipelineDesc &desc) noexcept;
        ~GraphicsPipelineDynamicRendering() noexcept override;

        [[nodiscard]] vk::Pipeline GetPipeline() const noexcept { return m_pipeline; }
    private:
        vk::Pipeline m_pipeline;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline {
    public:
        ComputePipeline(Vulkan::Context &ctx, std::string_view shader) noexcept;
        ~ComputePipeline() noexcept override;
        
        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) override;

        [[nodiscard]] vk::Pipeline GetPipeline(const std::string &name) const noexcept {
            const auto it = m_pipelines.find(name);
            return it == m_pipelines.end() ? VK_NULL_HANDLE : it->second;
        }

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] auto GetDstSet() const { return m_dst_set; }
        [[nodiscard]] PipelineResourceAccess GetResourceAccess(const vk::Pipeline pipeline) const noexcept {
            const auto it = m_resource_access.find(pipeline);
            return it != m_resource_access.end() ? it->second : PipelineResourceAccess{};
        }
    private:
        vk::PipelineLayout m_pipeline_layout;
        vk::ShaderModule m_shader_module;
        vk::DescriptorSetLayout m_dst_set_layout;
        vk::DescriptorSet m_dst_set;
        std::unordered_map<std::string, vk::Pipeline> m_pipelines;
        std::unordered_map<VkPipeline, PipelineResourceAccess> m_resource_access;
    };

    inline ComputePipeline::~ComputePipeline() noexcept {
        VulkanContext->DeleteObject(
            [
                pipelines = m_pipelines, 
                pipeline_layout = m_pipeline_layout,
                shader_module = m_shader_module,
                dst_set_layout = m_dst_set_layout
            ] (const vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto pipeline : pipelines | std::views::values)
                    device.destroy(pipeline);

                device.destroy(pipeline_layout);
                device.destroy(shader_module);
                device.destroy(dst_set_layout);
            });
    }

    inline GraphicsPipelineDynamicRendering::~GraphicsPipelineDynamicRendering() noexcept {
        VulkanContext->DeleteObject(
            [
                pipeline = m_pipeline, 
                pipeline_layout = m_pipeline_layout,
                shader_module = m_shader_module,
                dst_set_layout = m_dst_set_layout
            ] (const vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                device.destroy(pipeline_layout);
                device.destroy(pipeline);
                device.destroy(dst_set_layout);
                device.destroy(shader_module);
            });
    }

    inline GraphicsPipelineDefaultVk::~GraphicsPipelineDefaultVk() noexcept {
        VulkanContext->DeleteObject(
            [
                pipelines = m_pipelines, 
                pipeline_layout = m_pipeline_layout,
                renderpasses = m_renderpasses,
                shader_module = m_shader_module,
                dst_set_layout = m_dst_set_layout
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto pipeline : pipelines | std::views::values) {
                    device.destroy(pipeline);
                }
                for (const auto renderpass : renderpasses | std::views::values) {
                    device.destroy(renderpass);
                }
                device.destroy(pipeline_layout);
                device.destroy(dst_set_layout);
                device.destroy(shader_module);
            });
    }
}