#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include "DnmGL/Vulkan/Image.hpp"

namespace DnmGL::Vulkan {
    class GraphicsPipelineBase : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipelineBase(Vulkan::Context &context, const DnmGL::GraphicsPipelineDesc &desc) noexcept;

        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint32_t array_index) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint32_t array_index) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint32_t array_index) override;

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] auto GetDstSet() const { return m_dst_set; }
    protected:
        vk::Pipeline CreatePipeline(vk::RenderPass renderpass) noexcept;

        vk::DescriptorSetLayout m_dst_set_layout;
        vk::DescriptorSet m_dst_set;
        vk::PipelineLayout m_pipeline_layout;
        vk::ShaderModule m_shader_module;
    };

    class GraphicsPipelineDefaultVk final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDefaultVk(Vulkan::Context &context, const DnmGL::GraphicsPipelineDesc &desc) noexcept;
        ~GraphicsPipelineDefaultVk() noexcept;

        std::pair<vk::RenderPass, vk::Pipeline> GetOrCreateAttachmentOpVariant(AttachmentOps attachment_ops, bool presenting);
        vk::RenderPass GetRenderpass(uint32_t attachment_ops) noexcept;
        vk::Pipeline GetPipeline(vk::RenderPass render_pass) noexcept;
    private:
        vk::RenderPass CreateRenderpass(AttachmentOps renderpass_desc, bool presenting) noexcept;

        std::unordered_map<VkRenderPass, vk::Pipeline> m_pipelines;
        std::unordered_map<uint32_t, vk::RenderPass> m_renderpasses;
    };

    class GraphicsPipelineDynamicRendering final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDynamicRendering(Vulkan::Context &context, const DnmGL::GraphicsPipelineDesc &desc) noexcept;
        ~GraphicsPipelineDynamicRendering() noexcept;

        [[nodiscard]] vk::Pipeline GetPipeline() const noexcept { return m_pipeline; }
    private:
        vk::Pipeline m_pipeline;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline {
    public:
        ComputePipeline(Vulkan::Context &context, std::string_view shader) noexcept;
        ~ComputePipeline() noexcept;
        
        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint32_t array_index) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint32_t array_index) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint32_t array_index) override;

        [[nodiscard]] vk::Pipeline GetPipeline(const std::string &name) const noexcept { 
            auto it = m_pipelines.find(name);
            return it == m_pipelines.end() ? VK_NULL_HANDLE : it->second;
        }

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] auto GetDstSet() const { return m_dst_set; }
    private:
        vk::PipelineLayout m_pipeline_layout;
        vk::ShaderModule m_shader_module;
        vk::DescriptorSetLayout m_dst_set_layout;
        vk::DescriptorSet m_dst_set;
        std::unordered_map<std::string, vk::Pipeline> m_pipelines;
    };

    inline ComputePipeline::~ComputePipeline() noexcept {
        VulkanContext->DeleteObject(
            [
                pipelines = m_pipelines, 
                pipeline_layout = m_pipeline_layout,
                shader_module = m_shader_module,
                dst_set_layout = m_dst_set_layout
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto [_, pipeline] : pipelines)
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
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
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
                for (const auto [_, pipeline] : pipelines) {
                    device.destroy(pipeline);
                }
                for (const auto [_, renderpass] : renderpasses) {
                    device.destroy(renderpass);
                }
                device.destroy(pipeline_layout);
                device.destroy(dst_set_layout);
                device.destroy(shader_module);
            });
    }
}