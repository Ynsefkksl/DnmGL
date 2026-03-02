#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include "DnmGL/Vulkan/Pipeline.hpp"

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
        explicit CommandBuffer(Vulkan::Context& ctx);
        ~CommandBuffer() noexcept override {
            VulkanContext
                ->GetDevice().freeCommandBuffers(VulkanContext->GetCommandPool(), command_buffer);
        }
    
        void IBegin() override;
        void IEnd() override;

        void IBeginRendering(const BeginRenderingDesc& desc) override;
        void IEndRendering() override;

        void IBeginCopyPass() override {}
        void IEndCopyPass() override {
            DeferLayoutTranslation();
        }

        void IBeginComputePass() override {}
        void IEndComputePass() override {
            DeferLayoutTranslation();
        }

        void ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) override;
        void ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) override;
        void ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) override;
        void ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) override;
    
        void IComputeDispatch(const DnmGL::ComputePipeline *pipeline, std::string_view kernel, uint16_t x = 1, uint16_t y = 1, uint16_t z = 1) override;

        void IGenerateMipmaps(DnmGL::Image* image) override;
    
        void IBindVertexBuffer(const DnmGL::Buffer* buffer, uint64_t offset) override;
        void IBindIndexBuffer(const DnmGL::Buffer* buffer, uint64_t offset, DnmGL::IndexType index_type) override;
    
        void IDraw(uint32_t vertex_count, uint32_t instance_count) override;
        void IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) override;

        void ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) override;
        void ISetScissor(Uint2 extent, Uint2 offset) override;

        void IUploadData(DnmGL::Image *image, 
                        ImageSubresource subresource, 
                        const void* data, 
                        Uint3 copy_extent, 
                        Uint3 copy_offset) override;
        void IUploadData(DnmGL::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) override;

        void Barrier(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;
        void TransferImageLayout(std::span<const ImageBarrier> desc) const;
        void TransferImageLayout(std::span<const TransferImageLayoutNativeDesc> descs) const;

        void AddDeferLayoutTranslation(Vulkan::Image *image);
        void RemoveDeferLayoutTranslation(Vulkan::Image *image);
    private:
        void BarrierDefaultVk(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;
        void BarrierSync2(std::span<const Vulkan::BufferBarrier> buffer_barriers, std::span<const Vulkan::ImageBarrier> image_barriers) const;

        void TransferImageLayoutDefaultVk(std::span<const TransferImageLayoutNativeDesc> descs) const;
        void TransferImageLayoutSync2(std::span<const TransferImageLayoutNativeDesc> descs) const;

        void DeferLayoutTranslation();

        void BeginRenderingDefaultVk(const BeginRenderingDesc& desc);
        void BeginRenderingDynamicRendering(const BeginRenderingDesc& desc);
        static std::vector<vk::ClearValue> GetClearValues(const BeginRenderingDesc& begin_desc);

        void TranslateAttachmentLayouts(FramebufferBase &framebuffer);
        void TranslateSwapchainImageLayoutsInBegin() const;
        void TranslateSwapchainImageLayoutsInEnd() const;

        constexpr void BarrierForPipeline(vk::PipelineStageFlags dst_stage_flags, vk::AccessFlags dst_access_flags) noexcept;

        vk::CommandBuffer command_buffer;

        //process in BindPipeline or end
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
        DeferLayoutTranslation();
        prev_operation = CommandType::eNone;
    }
    
    inline void CommandBuffer::IEnd() {
        command_buffer.end();
    }
    
    inline void CommandBuffer::IBeginRendering(const BeginRenderingDesc& desc) {
        if (VulkanContext->GetSupportedFeatures().dynamic_rendering)
            BeginRenderingDynamicRendering(desc);
        else
            BeginRenderingDefaultVk(desc);

        prev_operation = CommandType::ePipeline;
    }

    inline void CommandBuffer::IDraw(const uint32_t vertex_count, const uint32_t instance_count) {
        command_buffer.draw(vertex_count, instance_count, 0, 0);
    }
    
    inline void CommandBuffer::IDrawIndexed(const uint32_t index_count, const uint32_t instance_count, const uint32_t vertex_offset) {
        command_buffer.drawIndexed(index_count, instance_count, 0, vertex_offset, 0);
    }

    inline void CommandBuffer::ISetViewport(const Float2 extent, const Float2 offset, float min_depth, const float max_depth) {
        command_buffer.setViewport(0, {vk::Viewport{}
                .setMinDepth(min_depth).setMinDepth(max_depth)
                .setHeight(-extent.y).setWidth(extent.x)
                .setX(offset.x).setY(offset.y + extent.y)
        });
    }

    inline void CommandBuffer::ISetScissor(const Uint2 extent, const Uint2 offset) {
        command_buffer.setScissor(0, { 
            vk::Rect2D{}
                .setOffset({static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y)})
                .setExtent({extent.x, extent.y})
        });
    }

    inline void CommandBuffer::Barrier(
        const std::span<const Vulkan::BufferBarrier> buffer_barriers,
        const std::span<const Vulkan::ImageBarrier> image_barriers) const {
        if (VulkanContext->GetSupportedFeatures().sync2) {
            BarrierSync2(buffer_barriers, image_barriers);
        }
        else {
            BarrierDefaultVk(buffer_barriers, image_barriers);
        }
    }

    inline void CommandBuffer::TransferImageLayout(
        const std::span<const TransferImageLayoutNativeDesc> descs) const {
        if (VulkanContext->GetSupportedFeatures().sync2) {
            TransferImageLayoutSync2(descs);
        }
        else {
            TransferImageLayoutDefaultVk(descs);
        }
    }

    inline void CommandBuffer::RemoveDeferLayoutTranslation(Vulkan::Image *image) {
        m_pending_layout_restore_images.erase(image);
    }

    inline std::vector<vk::ClearValue> CommandBuffer::GetClearValues(const BeginRenderingDesc& begin_desc) {
        //resolve_count = pipeline.ColorAttachmentCount()
        //if has msaa clear_value_count = pipeline.ColorAttachmentCount() + resolve_count + 1
        //if not has msaa clear_value_count = pipeline.ColorAttachmentCount() + 1
        const uint32_t clear_value_count = (begin_desc.pipeline->ColorAttachmentCount() * (1 + begin_desc.pipeline->HasMsaa())) + 1;

        std::vector<vk::ClearValue> clear_values{};
        clear_values.resize(clear_value_count);


        for (uint32_t i{}; const auto [r, g, b, a] : begin_desc.color_clear_values) {
            clear_values[i++] =
                vk::ClearColorValue(std::array{r, g, b, a});
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

    constexpr void CommandBuffer::BarrierForPipeline(const vk::PipelineStageFlags dst_stage_flags, const vk::AccessFlags dst_access_flags) noexcept {
        if (prev_operation != CommandType::eNone) {
            vk::PipelineStageFlags src_stage_flags{};
            vk::AccessFlags src_access_flags{};

            switch (prev_operation) {
                case CommandType::eNone: std::unreachable();
                case CommandType::eTransfer: {
                    src_stage_flags = vk::PipelineStageFlagBits::eTransfer;
                    src_access_flags = vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite;
                } break;
                case CommandType::ePipeline: {
                    src_stage_flags = prev_stage_flags;
                    src_access_flags = prev_access_flags;

                    if (!(src_access_flags & vk::AccessFlagBits::eShaderWrite)
                    && !(dst_access_flags & vk::AccessFlagBits::eShaderWrite)) {
                        return;
                    }
                } break;
            }

            const vk::MemoryBarrier barrier(
                src_access_flags,
                dst_access_flags
            );
            
            command_buffer.pipelineBarrier(
                src_stage_flags,
                dst_stage_flags,
                {}, 
                barrier, 
                {}, 
                {});
        }

        prev_stage_flags = dst_stage_flags;
        prev_access_flags = dst_access_flags;
    }
}