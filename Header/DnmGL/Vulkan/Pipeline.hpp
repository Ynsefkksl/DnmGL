#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include "DnmGL/Vulkan/Image.hpp"

namespace DnmGL::Vulkan {
    class GraphicsPipelineBase : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipelineBase(Vulkan::Context& context, const DnmGL::GraphicsPipelineDesc& desc) noexcept;

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] std::span<const vk::DescriptorSet, 4> GetDstSets() const { return m_dst_sets; }
        [[nodiscard]] auto GetPipelineStageFlags() const { return m_pipeline_stage_flags; }
        [[nodiscard]] auto GetAccessFlags() const { return m_access_flags; }
    protected:
        vk::Pipeline CreatePipeline(vk::RenderPass renderpass) noexcept;

        vk::PipelineLayout m_pipeline_layout;
        vk::DescriptorSet m_dst_sets[4];

        vk::PipelineStageFlags m_pipeline_stage_flags;
        vk::AccessFlags m_access_flags;
    };

    class GraphicsPipelineDefaultVk final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDefaultVk(Vulkan::Context& context, const DnmGL::GraphicsPipelineDesc& desc) noexcept;
        ~GraphicsPipelineDefaultVk() noexcept;

        std::pair<vk::RenderPass, vk::Pipeline> GetOrCreateAttachmentOpVariant(AttachmentOps attachment_ops, bool presenting);
        vk::RenderPass GetRenderpass(uint32_t attachment_ops) noexcept;
        vk::Pipeline GetPipeline(vk::RenderPass render_pass) noexcept;

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] std::span<const vk::DescriptorSet, 4> GetDstSets() const { return m_dst_sets; }
        [[nodiscard]] auto GetPipelineStageFlags() const { return m_pipeline_stage_flags; }
        [[nodiscard]] auto GetAccessFlags() const { return m_access_flags; }
    private:
        vk::RenderPass CreateRenderpass(AttachmentOps renderpass_desc, bool presenting) noexcept;
        //for renderpass
        std::unordered_map<VkRenderPass, vk::Pipeline> m_pipelines;
        std::unordered_map<uint32_t, vk::RenderPass> m_renderpasses;
    };

    class GraphicsPipelineDynamicRendering final : public GraphicsPipelineBase {
    public:
        GraphicsPipelineDynamicRendering(Vulkan::Context& context, const DnmGL::GraphicsPipelineDesc& desc) noexcept;
        ~GraphicsPipelineDynamicRendering() noexcept;

        [[nodiscard]] vk::Pipeline GetPipeline() const noexcept { return m_pipeline; }

        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] std::span<const vk::DescriptorSet, 4> GetDstSets() const { return m_dst_sets; }
        [[nodiscard]] auto GetPipelineStageFlags() const { return m_pipeline_stage_flags; }
        [[nodiscard]] auto GetAccessFlags() const { return m_access_flags; }
    private:
        vk::Pipeline m_pipeline;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline  {
    public:
        ComputePipeline(Vulkan::Context& context, const DnmGL::ComputePipelineDesc& desc) noexcept;
        ~ComputePipeline() noexcept;
        
        [[nodiscard]] auto GetPipeline() const { return m_pipeline; }
        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] std::span<const vk::DescriptorSet, 4> GetDstSets() const { return m_dst_sets; }

        [[nodiscard]] auto GetPipelineStageFlags() const { return m_pipeline_stage_flags; }
        [[nodiscard]] auto GetAccessFlags() const { return m_access_flags; }
    private:
        vk::DescriptorSet m_dst_sets[4];

        vk::PipelineStageFlags m_pipeline_stage_flags;
        vk::AccessFlags m_access_flags;

        vk::Pipeline m_pipeline;
        vk::PipelineLayout m_pipeline_layout;
    };

    inline ComputePipeline::~ComputePipeline() noexcept {
        VulkanContext->DeleteObject(
            [
                pipeline = m_pipeline, 
                pipeline_layout = m_pipeline_layout
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                device.destroy(pipeline_layout);
                device.destroy(pipeline);
            });
    }

    inline GraphicsPipelineDynamicRendering::~GraphicsPipelineDynamicRendering() noexcept {
        VulkanContext->DeleteObject(
            [
                pipeline = m_pipeline, 
                pipeline_layout = m_pipeline_layout
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                device.destroy(pipeline_layout);
                device.destroy(pipeline);
            });
    }

    inline GraphicsPipelineDefaultVk::~GraphicsPipelineDefaultVk() noexcept {
        VulkanContext->DeleteObject(
            [
                pipelines = m_pipelines, 
                pipeline_layout = m_pipeline_layout,
                renderpasses = m_renderpasses
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto [_, pipeline] : pipelines) {
                    device.destroy(pipeline);
                }
                for (const auto [_, renderpass] : renderpasses) {
                    device.destroy(renderpass);
                }
                device.destroy(pipeline_layout);
            });
    }
}