#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    struct BufferBarrier {
        Vulkan::Buffer* buffer;
        vk::PipelineStageFlags src_pipeline_stages;
        vk::PipelineStageFlags dst_pipeline_stages;
        vk::AccessFlags src_access;
        vk::AccessFlags dst_access;
    };

    struct ImageBarrier {
        Vulkan::Image* image;
        vk::ImageLayout new_image_layout;
        vk::PipelineStageFlags src_pipeline_stages;
        vk::PipelineStageFlags dst_pipeline_stages;
        vk::AccessFlags src_access;
        vk::AccessFlags dst_access;
    };

    struct TransferImageLayoutNativeDesc {
        vk::Image image;
        vk::ImageAspectFlags image_aspect;
        vk::ImageLayout old_image_layout;
        vk::ImageLayout new_image_layout;
        vk::PipelineStageFlags src_pipeline_stages;
        vk::PipelineStageFlags dst_pipeline_stages;
        vk::AccessFlags src_access;
        vk::AccessFlags dst_access;
    };

    class CommandBuffer final : public DnmGL::CommandBuffer {
    public:
        CommandBuffer(Vulkan::Context& context);
        ~CommandBuffer() noexcept {
            VulkanContext
                ->GetDevice().freeCommandBuffers(VulkanContext->GetCommandPool(), command_buffer);
        }
    
        void IBegin() override;
        void IEnd() override;

        void IBeginRendering(const BeginRenderingDesc& desc) override;
        void IEndRendering() override;

        void IBeginCopyPass() override {}
        void IEndCopyPass() override {
            ProcessPendingLayoutRestores();
        }

        void IBeginComputePass() override {}
        void IEndComputePass() override {
            ProcessPendingLayoutRestores();
        }

        void ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) override;
        void ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) override;
        void ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) override;
        void ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) override;
    
        void IDispatch(uint32_t x = 1, uint32_t y = 1, uint32_t z = 1) override;

        void IBindPipeline(const DnmGL::ComputePipeline* pipeline) override;

        void IGenerateMipmaps(DnmGL::Image* image) override;
    
        void IBindVertexBuffer(const DnmGL::Buffer* buffer, uint64_t offset) override;
        void IBindIndexBuffer(const DnmGL::Buffer* buffer, uint64_t offset, DnmGL::IndexType index_type) override;
    
        void IDraw(uint32_t vertex_count, uint32_t instance_count) override;
        void IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) override;
    
        void IPushConstant(const DnmGL::GraphicsPipeline* pipeline, DnmGL::ShaderStageFlags pipeline_stage, uint32_t offset, uint32_t size, const void *ptr) override;
        void IPushConstant(const DnmGL::ComputePipeline* pipeline, uint32_t offset, uint32_t size, const void *ptr) override;

        void ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) override;
        void ISetScissor(Uint2 extent, Uint2 offset) override;

        void IUploadData(DnmGL::Image *image, 
                        const ImageSubresource& subresource, 
                        const void* data, 
                        Uint3 copy_extent, 
                        Uint3 copy_offset) override;
        void IUploadData(DnmGL::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) override;

        void Barrier(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;
        void BufferBarrier(std::span<const ImageBarrier> desc) const;
        void TransferImageLayout(std::span<const ImageBarrier> desc) const;
        void TransferImageLayout(std::span<const TransferImageLayoutNativeDesc> desc) const;

        void DeferLayoutRestore(Vulkan::Image *image);
        void CancelLayoutRestore(Vulkan::Image *image);
    private:
        void BarrierDefaultVk(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;
        void BarrierSync2(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;

        void TransferImageLayoutDefaultVk(std::span<const TransferImageLayoutNativeDesc> descs) const;
        void TransferImageLayoutSync2(std::span<const TransferImageLayoutNativeDesc> descs) const;

        void ProcessPendingLayoutRestores();

        void BeginRenderingDefaultVk(const BeginRenderingDesc& desc);
        void BeginRenderingDynamicRendering(const BeginRenderingDesc& desc);
        std::vector<vk::ClearValue> GetClearValues(const BeginRenderingDesc& begin_desc);

        void TranslateAttachmentLayouts(FramebufferBase &framebuffer);
        void TranslateSwapchainImageLayoutsInBegin();
        void TranslateSwapchainImageLayoutsInEnd();

        constexpr void BarrierForPipeline(vk::PipelineStageFlags stage_flags, vk::AccessFlags access_flags) noexcept;

        vk::CommandBuffer command_buffer;

        //procress in BindPipeline or end
        std::unordered_set<Vulkan::Image *> m_pending_layout_restore_images;

        //this is unnecessary
        enum class CommandType {
            eNone,
            eTransfer,
            ePipeline,
        } prev_operation{};

        vk::PipelineStageFlags prev_stage_flags{};
        vk::AccessFlags prev_access_flags{};

        friend Vulkan::Context;
    };
    
    inline void CommandBuffer::IBegin() {
        command_buffer.reset();
        command_buffer.begin(vk::CommandBufferBeginInfo{});
        ProcessPendingLayoutRestores();
        prev_operation = CommandType::eNone;
    }
    
    inline void CommandBuffer::IEnd() {
        ProcessPendingLayoutRestores();
        command_buffer.end();
    }
    
    inline void CommandBuffer::IBeginRendering(const BeginRenderingDesc& desc) {
        if (VulkanContext->GetSupportedFeatures().dynamic_rendering)
            BeginRenderingDynamicRendering(desc);
        else
            BeginRenderingDefaultVk(desc);

        prev_operation = CommandType::ePipeline;
    }

    inline void CommandBuffer::IDraw(uint32_t vertex_count, uint32_t instance_count) {
        command_buffer.draw(vertex_count, instance_count, 0, 0);
    }
    
    inline void CommandBuffer::IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) {
        command_buffer.drawIndexed(index_count, instance_count, 0, vertex_offset, 0);
    }

    inline void CommandBuffer::ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) {
        command_buffer.setViewport(0, {vk::Viewport{}
                .setMinDepth(max_depth).setMinDepth(max_depth)
                .setHeight(-extent.y).setWidth(extent.x)
                .setX(offset.x).setY(offset.y + extent.y)
        });
    }

    inline void CommandBuffer::ISetScissor(Uint2 extent, Uint2 offset) {
        command_buffer.setScissor(0, { 
            vk::Rect2D{}
                .setOffset({static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y)})
                .setExtent({extent.x, extent.y})
        });
    }

    inline void CommandBuffer::IDispatch(uint32_t x, uint32_t y, uint32_t z) {
        command_buffer.dispatch(x, y, z);
    }

    inline void CommandBuffer::Barrier(
        std::span<const Vulkan::BufferBarrier> buffer_barriers, 
        std::span<const Vulkan::ImageBarrier> image_barriers) const {
        if (VulkanContext->GetSupportedFeatures().sync2) {
            BarrierSync2(buffer_barriers, image_barriers);
        }
        else {
            BarrierDefaultVk(buffer_barriers, image_barriers);
        }
    }

    inline void CommandBuffer::TransferImageLayout(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        if (VulkanContext->GetSupportedFeatures().sync2) {
            TransferImageLayoutSync2(descs);
        }
        else {
            TransferImageLayoutDefaultVk(descs);
        }
    }

    inline void CommandBuffer::CancelLayoutRestore(Vulkan::Image *image) {
        m_pending_layout_restore_images.erase(image);
    }

    inline std::vector<vk::ClearValue> CommandBuffer::GetClearValues(const BeginRenderingDesc& begin_desc) {
        //resolve_count = pipeline.ColorAttachmentCount()
        //if has msaa clear_value_count = pipeline.ColorAttachmentCount() + resolve_count + 1
        //if not has msaa clear_value_count = pipeline.ColorAttachmentCount() + 1
        const uint32_t clear_value_count = (begin_desc.pipeline->ColorAttachmentCount() * (1 + begin_desc.pipeline->HasMsaa())) + 1;

        std::vector<vk::ClearValue> clear_values{};
        clear_values.resize(clear_value_count);
        
        uint32_t i{};
        for (const auto value : begin_desc.color_clear_values) {
            clear_values[i++] =
                vk::ClearColorValue(std::array{value.r, value.g, value.b, value.a});
        }
        //resolve images
        if (begin_desc.pipeline->HasMsaa())
            for (const auto i : Counter(begin_desc.pipeline->ColorAttachmentCount())) {
                clear_values[begin_desc.pipeline->ColorAttachmentCount() + i] =
                    vk::ClearColorValue(std::array{0, 0, 0, 0});
            }
        
        if (begin_desc.attachment_ops.depth_load == AttachmentLoadOp::eClear) {
            clear_values[clear_value_count - 1]
                = vk::ClearDepthStencilValue(begin_desc.depth_stencil_clear_value.depth, begin_desc.depth_stencil_clear_value.stencil);
        }
        return clear_values;
    }

    inline constexpr void CommandBuffer::BarrierForPipeline(vk::PipelineStageFlags pipeline_stage_flags, vk::AccessFlags pipeline_access_flags) noexcept {
        if (prev_operation != CommandType::eNone) {
            vk::PipelineStageFlags stage_flags{};
            vk::AccessFlags access_flags{};

            switch (prev_operation) {
                case CommandType::eNone: std::unreachable(); break;
                case CommandType::eTransfer: {
                    stage_flags |= vk::PipelineStageFlagBits::eTransfer;
                    access_flags |= vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite;
                }
                case CommandType::ePipeline: {
                    stage_flags |= prev_stage_flags;
                    access_flags |= prev_access_flags;
                } break;
            }

            const vk::MemoryBarrier barrier(
                {},
                pipeline_access_flags
            );
            
            command_buffer.pipelineBarrier(
                {}, 
                pipeline_stage_flags, 
                {}, 
                barrier, 
                {}, 
                {});
        }

        prev_stage_flags = pipeline_stage_flags;
        prev_access_flags = pipeline_access_flags;
    }
}