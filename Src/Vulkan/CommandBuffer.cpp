#include "DnmGL/Vulkan/CommandBuffer.hpp"
#include "DnmGL/Vulkan/Pipeline.hpp"
#include "DnmGL/Vulkan/Buffer.hpp"
#include "DnmGL/Vulkan/Image.hpp"
#include "DnmGL/Vulkan/Framebuffer.hpp"

namespace DnmGL::Vulkan {
    CommandBuffer::CommandBuffer(Vulkan::Context& ctx)
        : DnmGL::CommandBuffer(ctx) {
        vk::CommandBufferAllocateInfo alloc_descs;
        alloc_descs.setCommandBufferCount(1)
                    .setCommandPool(VulkanContext->GetCommandPool())
                    .setLevel(vk::CommandBufferLevel::ePrimary);
    
        command_buffer = VulkanContext->GetDevice().allocateCommandBuffers(alloc_descs)[0];
    }
    
    void CommandBuffer::IBindPipeline(const DnmGL::ComputePipeline* pipeline) {
        const auto* typed_pipeline = static_cast<const Vulkan::ComputePipeline *>(pipeline);

        BarrierForPipeline(typed_pipeline->GetPipelineStageFlags(), typed_pipeline->GetAccessFlags());

        ProcessPendingLayoutRestores();

        command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute, 
                        typed_pipeline->GetPipelineLayout(),
                        0,
                        typed_pipeline->GetDstSets(),
                        {});

        command_buffer.bindPipeline(
            vk::PipelineBindPoint::eCompute, 
            typed_pipeline->GetPipeline()
        );
        prev_operation = CommandType::ePipeline;
    }

    void CommandBuffer::ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) {
        auto* typed_src_image = static_cast<Vulkan::Image *>(desc.src_image);
        auto* typed_dst_buffer = static_cast<Vulkan::Buffer *>(desc.dst_buffer);

        const Vulkan::BufferBarrier buffer_barrier{
            .buffer = typed_dst_buffer,
            .src_pipeline_stages = typed_dst_buffer->prev_pipeline_stage,
            .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
            .src_access = typed_dst_buffer->prev_access,
            .dst_access = vk::AccessFlagBits::eTransferWrite,
        };
        Vulkan::ImageBarrier image_barrier;

        const auto image_barrier_needed = 
            typed_src_image->GetImageLayout() != vk::ImageLayout::eTransferSrcOptimal
            && typed_src_image->prev_access != vk::AccessFlagBits::eTransferRead
            && typed_src_image->prev_access != vk::AccessFlagBits::eShaderRead
            && typed_src_image->prev_access != vk::AccessFlagBits::eColorAttachmentRead
            && typed_src_image->prev_access != vk::AccessFlagBits::eDepthStencilAttachmentRead;
        
        if (image_barrier_needed) {
            image_barrier.image = typed_src_image;
            image_barrier.new_image_layout = vk::ImageLayout::eTransferSrcOptimal;
            image_barrier.dst_access = vk::AccessFlagBits::eTransferRead;
            image_barrier.src_access = typed_src_image->prev_access;
            image_barrier.dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer;
            image_barrier.src_pipeline_stages = typed_src_image->prev_pipeline_stage;

            DeferLayoutRestore(typed_src_image);
        }

        Barrier(std::span(&buffer_barrier, 1), std::span(&image_barrier, image_barrier_needed));

        const vk::BufferImageCopy buffer_image_copy {
            desc.buffer_offset,
            {},
            {},
            vk::ImageSubresourceLayers(
                typed_src_image->GetAspect(),
                desc.image_subresource.base_mipmap,
                desc.image_subresource.base_layer,
                desc.image_subresource.layer_count
            ),
            vk::Offset3D(desc.copy_offset.x, desc.copy_offset.y, desc.copy_offset.z),
            vk::Extent3D(desc.copy_extent.x, desc.copy_extent.y, desc.copy_extent.z)
        };

        command_buffer.copyImageToBuffer(
            typed_src_image->GetImage(), 
            typed_src_image->GetImageLayout(),
            typed_dst_buffer->GetBuffer(),
            buffer_image_copy
        );
        prev_operation = CommandType::eTransfer;
    }

    void CommandBuffer::ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) {
        auto* typed_src_image = static_cast<Vulkan::Image *>(desc.src_image);
        auto* typed_dst_image = static_cast<Vulkan::Image *>(desc.dst_image);

        {
            Vulkan::ImageBarrier image_barrier[2] {
                Vulkan::ImageBarrier{
                    .image = typed_dst_image,
                    .new_image_layout = vk::ImageLayout::eTransferDstOptimal,
                    .src_pipeline_stages = typed_dst_image->prev_pipeline_stage,
                    .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                    .src_access = typed_dst_image->prev_access,
                    .dst_access = vk::AccessFlagBits::eTransferWrite,
                }
            };
    
            const auto src_image_barrier_needed = 
                typed_src_image->GetImageLayout() != vk::ImageLayout::eTransferSrcOptimal
                && typed_src_image->prev_access != vk::AccessFlagBits::eTransferRead
                && typed_src_image->prev_access != vk::AccessFlagBits::eShaderRead
                && typed_src_image->prev_access != vk::AccessFlagBits::eColorAttachmentRead
                && typed_src_image->prev_access != vk::AccessFlagBits::eDepthStencilAttachmentRead;
            
            if (src_image_barrier_needed) {
                image_barrier[1].image = typed_src_image;
                image_barrier[1].new_image_layout = vk::ImageLayout::eTransferSrcOptimal;
                image_barrier[1].dst_access = vk::AccessFlagBits::eTransferRead;
                image_barrier[1].src_access = typed_src_image->prev_access;
                image_barrier[1].dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer;
                image_barrier[1].src_pipeline_stages = typed_src_image->prev_pipeline_stage;
    
                DeferLayoutRestore(typed_src_image);
            }
    
            DeferLayoutRestore(typed_dst_image);
    
            Barrier({}, std::span(image_barrier, src_image_barrier_needed + 1));
        }

        const vk::ImageCopy image_copy(
            vk::ImageSubresourceLayers(
                typed_src_image->GetAspect(),
                desc.src_image_subresource.base_mipmap,
                desc.src_image_subresource.base_layer,
                desc.src_image_subresource.layer_count
            ),
            vk::Offset3D(desc.src_offset.x, desc.src_offset.y, desc.src_offset.z),
            vk::ImageSubresourceLayers(
                typed_dst_image->GetAspect(),
                desc.dst_image_subresource.base_mipmap,
                desc.dst_image_subresource.base_layer,
                desc.dst_image_subresource.layer_count
            ),
            vk::Offset3D(desc.dst_offset.x, desc.dst_offset.y, desc.dst_offset.z),
            vk::Extent3D(desc.copy_extent.x, desc.copy_extent.y, desc.copy_extent.z)
        );
        
        command_buffer.copyImage(
            typed_src_image->GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            typed_dst_image->GetImage(),
            vk::ImageLayout::eTransferDstOptimal,
            image_copy
        );

        prev_operation = CommandType::eTransfer;
    }

    void CommandBuffer::ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc &desc) {
        auto *typed_src_buffer = static_cast<Vulkan::Buffer *>(desc.src_buffer);
        auto *typed_dst_buffer = static_cast<Vulkan::Buffer *>(desc.dst_buffer);

        {
            Vulkan::BufferBarrier buffer_barrier[2] {
                Vulkan::BufferBarrier{
                    .buffer = typed_dst_buffer,
                    .src_pipeline_stages = typed_dst_buffer->prev_pipeline_stage,
                    .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                    .src_access = typed_dst_buffer->prev_access,
                    .dst_access = vk::AccessFlagBits::eTransferWrite
                }
            };
    
            const auto src_buffer_barrier_needed = 
                typed_src_buffer->prev_access != vk::AccessFlagBits::eTransferRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eShaderRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eUniformRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eVertexAttributeRead;
            
            if (src_buffer_barrier_needed) {
                buffer_barrier[1].buffer = typed_src_buffer;
                buffer_barrier[1].dst_access = vk::AccessFlagBits::eTransferRead;
                buffer_barrier[1].src_access = typed_src_buffer->prev_access;
                buffer_barrier[1].dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer;
                buffer_barrier[1].src_pipeline_stages = typed_src_buffer->prev_pipeline_stage;
            }
    
            Barrier({buffer_barrier, src_buffer_barrier_needed + 1u}, {});
        }

        const vk::BufferCopy buffer_copy {
            desc.src_offset,
            desc.dst_offset,
            desc.copy_size
        };

        command_buffer.copyBuffer(
            typed_src_buffer->GetBuffer(), 
            typed_dst_buffer->GetBuffer(), 
            buffer_copy);

        prev_operation = CommandType::eTransfer;
    }

    void CommandBuffer::ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) {
        auto* typed_src_buffer = static_cast<Vulkan::Buffer*>(desc.src_buffer);
        auto* typed_dst_image = static_cast<Vulkan::Image*>(desc.dst_image);

        {
            Vulkan::ImageBarrier image_barrier[1] {
                Vulkan::ImageBarrier{
                    .image = typed_dst_image,
                    .new_image_layout = vk::ImageLayout::eTransferDstOptimal,
                    .src_pipeline_stages = typed_dst_image->prev_pipeline_stage,
                    .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                    .src_access = typed_dst_image->prev_access,
                    .dst_access = vk::AccessFlagBits::eTransferWrite,
                }
            };

            Vulkan::BufferBarrier buffer_barrier[1];
    
            const auto buffer_barrier_needed = 
                typed_src_buffer->prev_access != vk::AccessFlagBits::eTransferRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eShaderRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eUniformRead
                && typed_src_buffer->prev_access != vk::AccessFlagBits::eVertexAttributeRead;
            
            if (buffer_barrier_needed) {
                buffer_barrier->buffer = typed_src_buffer;
                buffer_barrier->dst_access = vk::AccessFlagBits::eTransferRead;
                buffer_barrier->src_access = typed_src_buffer->prev_access;
                buffer_barrier->dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer;
                buffer_barrier->src_pipeline_stages = typed_src_buffer->prev_pipeline_stage;
            }
    
            DeferLayoutRestore(typed_dst_image);

            Barrier(std::span(buffer_barrier, buffer_barrier_needed), image_barrier);
        }
        
        const vk::BufferImageCopy buffer_image_copy {
            desc.buffer_offset,
            0,
            0,
            vk::ImageSubresourceLayers(
                typed_dst_image->GetAspect(),
                desc.image_subresource.base_mipmap,
                desc.image_subresource.base_layer,
                desc.image_subresource.layer_count
            ),
            vk::Offset3D(desc.copy_offset.x, desc.copy_offset.y, desc.copy_offset.z),
            vk::Extent3D(desc.copy_extent.x, desc.copy_extent.y, desc.copy_extent.z)
        };

        command_buffer.copyBufferToImage(
            typed_src_buffer->GetBuffer(), 
            typed_dst_image->GetImage(), 
            vk::ImageLayout::eTransferDstOptimal, 
            buffer_image_copy);
            
        prev_operation = CommandType::eTransfer;
    }

    void CommandBuffer::TransferImageLayout(
        std::span<const ImageBarrier> descs) const {
        std::vector<TransferImageLayoutNativeDesc> barriers{};
        barriers.reserve(descs.size());

        for (const auto& desc : descs) {
            if (desc.new_image_layout == desc.image->GetImageLayout()) {
                continue;
            }
            barriers.emplace_back(
                desc.image->GetImage(),
                desc.image->GetAspect(),
                desc.image->GetImageLayout(),
                desc.new_image_layout,
                desc.src_pipeline_stages,
                desc.dst_pipeline_stages,
                desc.src_access,
                desc.dst_access
            );

            desc.image->m_image_layout = desc.new_image_layout;
        }

        TransferImageLayout(barriers);
    }

    void CommandBuffer::TransferImageLayoutDefaultVk(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        for (const auto& desc : descs) {
            const vk::ImageMemoryBarrier barrier {
                vk::ImageMemoryBarrier(
                desc.src_access,
                desc.dst_access,
                desc.old_image_layout,
                desc.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                desc.image,
                vk::ImageSubresourceRange( // sub resource
                        desc.image_aspect, // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    ))
            };

            command_buffer.pipelineBarrier(
                desc.src_pipeline_stages,
                desc.dst_pipeline_stages, 
                {}, 
                {}, 
                {}, 
                barrier);
        }
    }
    
    void CommandBuffer::TransferImageLayoutSync2(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        std::vector<vk::ImageMemoryBarrier2> barriers;
        barriers.reserve(descs.size());

        for (const auto& desc : descs) {
            barriers.emplace_back(
                static_cast<vk::PipelineStageFlags2>((uint32_t)desc.src_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)desc.src_access),
                static_cast<vk::PipelineStageFlags2>((uint32_t)desc.dst_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)desc.dst_access),
                desc.old_image_layout,
                desc.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                desc.image,
                vk::ImageSubresourceRange( // sub resource
                        desc.image_aspect, // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    )
            );
        }

        const vk::DependencyInfo dependency_descs {
            {},
            {},
            {},
            {},
            {},
            static_cast<uint32_t>(barriers.size()),
            barriers.data()
        };

        command_buffer.pipelineBarrier2KHR(
            dependency_descs, VulkanContext->GetDispatcher());
    }

    void CommandBuffer::IGenerateMipmaps(DnmGL::Image* image) {
        auto& image_desc = image->GetDesc();
        auto* typed_image = static_cast<Vulkan::Image*>(image);

        const auto& barrier = [
            cmd_buffer = command_buffer, 
            vk_image = typed_image->GetImage(),
            vk_aspect = typed_image->GetAspect()
        ] (
            vk::AccessFlags src_access,
            vk::AccessFlags dst_access,
            vk::PipelineStageFlags src_pipeline_stage,
            vk::PipelineStageFlags dst_pipeline_stage,
            vk::ImageLayout old_image_layout,
            vk::ImageLayout new_image_layout,
            uint32_t mipmap_level,
            uint32_t array_layer
        ) {
            const vk::ImageMemoryBarrier image_barrier(
                src_access,
                dst_access,
                old_image_layout,
                new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                vk_image,
                vk::ImageSubresourceRange( // sub resource
                        vk_aspect, // aspect
                        mipmap_level, // base mipmap
                        1, // mipmap count
                        array_layer, // base layer
                        1 // layer count
                    )
            );

            cmd_buffer.pipelineBarrier(
                src_pipeline_stage,
                dst_pipeline_stage,
                {},
                {},
                {},
                {image_barrier}
            );
        };

        ImageBarrier layout_transfer_barrier{
            typed_image,
            vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            vk::AccessFlagBits::eTransferWrite,
        };

        //transfer whole image layout
        TransferImageLayout(std::span(&layout_transfer_barrier, 1));

        for (uint32_t array_index = 0; array_index < image_desc.extent.z; ++array_index) {
            int32_t mip_width = image_desc.extent.x;
            int32_t mip_height = image_desc.extent.y;

            for (uint32_t mipmap_index = 1; mipmap_index < image_desc.mipmap_levels; ++mipmap_index) {
                barrier(
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eTransferSrcOptimal,
                    mipmap_index - 1,
                    array_index
                );

                vk::ImageBlit image_blit(
                    vk::ImageSubresourceLayers(
                        typed_image->GetAspect(),
                        mipmap_index - 1,
                        array_index,
                        1
                    ),
                    {vk::Offset3D{}, {mip_width, mip_height, 1}},
                    vk::ImageSubresourceLayers(
                        typed_image->GetAspect(),
                        mipmap_index,
                        array_index,
                        1
                    ),
                    {vk::Offset3D{}, {
                        mip_width > 1 ? mip_width / 2 : 1,
                        mip_height > 1 ? mip_height / 2 : 1,
                        1}}
                );

                command_buffer.blitImage(
                    typed_image->GetImage(),
                    vk::ImageLayout::eTransferSrcOptimal,
                    typed_image->GetImage(),
                    vk::ImageLayout::eTransferDstOptimal,
                    {image_blit},
                    vk::Filter::eLinear
                );

                if (mip_width > 1) mip_width /= 2;
                if (mip_height > 1) mip_height /= 2;
            }

            barrier(
                vk::AccessFlagBits::eTransferWrite,
                {},
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                image_desc.mipmap_levels - 1,
                array_index
            );
        }

        //old image layout
        typed_image->m_image_layout = vk::ImageLayout::eTransferSrcOptimal;

        layout_transfer_barrier = {
            typed_image,
            typed_image->GetIdealImageLayout(),
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::AccessFlagBits::eTransferRead,
            {}
        };

        //transfer whole images, layout
        Barrier({}, std::span(&layout_transfer_barrier, 1));
        prev_operation = CommandType::eTransfer;
    }

    void CommandBuffer::IBindVertexBuffer(const DnmGL::Buffer *buffer, uint64_t offset) {
        command_buffer.bindVertexBuffers(
            0, 
            {static_cast<const Vulkan::Buffer *>(buffer)->GetBuffer()}, 
            {offset});
    }

    void CommandBuffer::IBindIndexBuffer(const DnmGL::Buffer *buffer, uint64_t offset, DnmGL::IndexType index_type) {
        command_buffer.bindIndexBuffer(
            static_cast<const Vulkan::Buffer *>(buffer)->GetBuffer(), 
            offset, 
            static_cast<vk::IndexType>(index_type));
    }

    void CommandBuffer::IUploadData(DnmGL::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) {
        auto *typed_buffer = static_cast<Vulkan::Buffer *>(buffer);

        //maybe this removed and upload func move to DnmGL.hpp for inlining
        if (size < 65536) {
            auto *typed_buffer = static_cast<Vulkan::Buffer *>(buffer);
            
            Vulkan::BufferBarrier buffer_barrier[1] {
                Vulkan::BufferBarrier{
                    .buffer = typed_buffer,
                    .src_pipeline_stages = typed_buffer->prev_pipeline_stage,
                    .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                    .src_access = typed_buffer->prev_access,
                    .dst_access = vk::AccessFlagBits::eTransferWrite
                }
            };

            Barrier(buffer_barrier, {});
            
            command_buffer.updateBuffer(typed_buffer->GetBuffer(), offset, size, data);
            prev_operation = CommandType::eTransfer;
            return;
        }

        //No problem, the Vulkan object is destroyed at the start of ExecuteCommands() or Render()
        Vulkan::Buffer staging_buffer(*VulkanContext, {
            .element_size = size,
            .element_count = 1,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .usage_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr(), data, size);

        ICopyBufferToBuffer({
            .src_buffer = &staging_buffer,
            .dst_buffer = typed_buffer,
            .src_offset = offset,
            .dst_offset = 0,
            .copy_size = size,
        });
    }

    void CommandBuffer::IUploadData(
        DnmGL::Image *image, 
        const ImageSubresource& subresource, 
        const void* data, 
        Uint3 copy_extent, 
        Uint3 copy_offset) {

        const auto copy_size = copy_extent.x * copy_extent.y * copy_extent.z * GetFormatSize(image->GetDesc().format);
        //No problem, the Vulkan object is destroyed at the start of ExecuteCommands() or Render()
        Vulkan::Buffer staging_buffer(*VulkanContext, {
            .element_size = copy_size,
            .element_count = 1,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .usage_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr(), data, copy_size);

        ICopyBufferToImage({
            .src_buffer = &staging_buffer,
            .dst_image = image,
            .image_subresource = subresource,
            .buffer_offset = 0,
            .copy_offset = copy_offset,
            .copy_extent = copy_extent
        });
    }

    void CommandBuffer::IEndRendering() {
        if (VulkanContext->GetSupportedFeatures().dynamic_rendering) {
            command_buffer.endRenderingKHR(VulkanContext->GetDispatcher());
            TranslateSwapchainImageLayoutsInEnd();
        }
        else
            command_buffer.endRenderPass();

        ProcessPendingLayoutRestores();

        if (active_framebuffer) {
            auto* typed_pipeline = static_cast<Vulkan::FramebufferDefaultVk *>(active_framebuffer);
            auto user_color_images = typed_pipeline->GetUserColorAttachments();
            auto* user_depth_stencil_image = typed_pipeline->GetUserDepthStencilAttachment();
    
            std::vector<ImageBarrier> transfer_image_layout;
            transfer_image_layout.reserve(user_color_images.size() + 2);
    
            for (auto *image : user_color_images) {
                transfer_image_layout.emplace_back(
                    image,
                    GetIdealImageLayout(image->GetDesc().usage_flags),
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits{}
                );
            }
            
            if (user_depth_stencil_image != nullptr) {
                transfer_image_layout.emplace_back(
                    user_depth_stencil_image,
                    GetIdealImageLayout(user_depth_stencil_image->GetDesc().usage_flags),
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits{}
                );
            }
    
            TransferImageLayout(transfer_image_layout);
        }
    }

    // TODO: fix this
    void CommandBuffer::ProcessPendingLayoutRestores() {
        if (!m_pending_layout_restore_images.size()) {
            return;
        }

        std::vector<ImageBarrier> image_layout_transfer_desc;
        image_layout_transfer_desc.reserve(m_pending_layout_restore_images.size());

        for (auto* image : m_pending_layout_restore_images) {
            const auto dst_access_flag = 
                image->GetImageLayout() == vk::ImageLayout::eTransferSrcOptimal ? vk::AccessFlagBits::eTransferRead : vk::AccessFlagBits::eTransferWrite;

            image_layout_transfer_desc.emplace_back(
                image,
                image->GetIdealImageLayout(),
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                dst_access_flag,
                vk::AccessFlagBits{}
            );
        }
        TransferImageLayout(image_layout_transfer_desc);
        m_pending_layout_restore_images.clear();
    }

    void CommandBuffer::BarrierDefaultVk(
        std::span<const Vulkan::BufferBarrier> buffer_barriers, 
        std::span<const Vulkan::ImageBarrier> image_barriers) const {

        vk::PipelineStageFlags src_stage_flags;
        vk::PipelineStageFlags dst_stage_flags;

        std::vector<vk::BufferMemoryBarrier> vk_buffer_barriers;
        std::vector<vk::ImageMemoryBarrier> vk_image_barriers;
        vk_buffer_barriers.reserve(buffer_barriers.size());
        vk_image_barriers.reserve(image_barriers.size());

        for (const auto &barrier : buffer_barriers) {
            auto *typed_buffer = barrier.buffer;
            
            src_stage_flags |= barrier.src_pipeline_stages;
            dst_stage_flags |= barrier.dst_pipeline_stages;

            vk_buffer_barriers.emplace_back(
                barrier.src_access,
                barrier.dst_access,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                typed_buffer->GetBuffer(),
                0,
                typed_buffer->GetDesc().element_count * typed_buffer->GetDesc().element_size
            );

            typed_buffer->prev_access = barrier.dst_access;
            typed_buffer->prev_pipeline_stage = barrier.dst_pipeline_stages;
        }

        for (const auto &barrier : image_barriers) {
            auto *typed_image = barrier.image;

            src_stage_flags |= barrier.src_pipeline_stages;
            dst_stage_flags |= barrier.dst_pipeline_stages;

            vk_image_barriers.emplace_back(
                barrier.src_access,
                barrier.dst_access,
                typed_image->GetImageLayout(),
                barrier.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                typed_image->GetImage(),
                vk::ImageSubresourceRange( // sub resource
                        typed_image->GetAspect(), // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    )
            );

            typed_image->m_image_layout = barrier.new_image_layout;

            typed_image->prev_access = barrier.dst_access;
            typed_image->prev_pipeline_stage = barrier.dst_pipeline_stages;
        }

        command_buffer.pipelineBarrier(
            src_stage_flags, 
            dst_stage_flags, 
            {}, 
            {}, 
            vk_buffer_barriers, 
            vk_image_barriers);   
    }

    void CommandBuffer::BarrierSync2(
        std::span<const Vulkan::BufferBarrier> buffer_barriers, 
        std::span<const Vulkan::ImageBarrier> image_barriers) const {

        std::vector<vk::BufferMemoryBarrier2KHR> vk_buffer_barriers;
        std::vector<vk::ImageMemoryBarrier2KHR> vk_image_barriers;
        vk_buffer_barriers.reserve(buffer_barriers.size());
        vk_image_barriers.reserve(image_barriers.size());

        for (const auto &barrier : buffer_barriers) {
            auto *typed_buffer = barrier.buffer;
            vk_buffer_barriers.emplace_back(
                static_cast<vk::PipelineStageFlags2>((uint32_t)barrier.src_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)barrier.src_access),
                static_cast<vk::PipelineStageFlags2>((uint32_t)barrier.dst_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)barrier.dst_access),
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                typed_buffer->GetBuffer(),
                0,
                typed_buffer->GetDesc().element_count * typed_buffer->GetDesc().element_size
            );

            typed_buffer->prev_access = barrier.dst_access;
            typed_buffer->prev_pipeline_stage = barrier.dst_pipeline_stages;
        }

        for (const auto &barrier : image_barriers) {
            auto *typed_image = barrier.image;
            vk_image_barriers.emplace_back(
                static_cast<vk::PipelineStageFlags2>((uint32_t)barrier.src_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)barrier.src_access),
                static_cast<vk::PipelineStageFlags2>((uint32_t)barrier.dst_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)barrier.dst_access),
                typed_image->GetImageLayout(),
                barrier.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                typed_image->GetImage(),
                vk::ImageSubresourceRange( // sub resource
                        typed_image->GetAspect(), // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    )
            );

            typed_image->m_image_layout = barrier.new_image_layout;

            typed_image->prev_access = barrier.dst_access;
            typed_image->prev_pipeline_stage = barrier.dst_pipeline_stages;
        }

        const vk::DependencyInfo dependency_desc {
            {},
            {},
            {},
            static_cast<uint32_t>(vk_buffer_barriers.size()),
            vk_buffer_barriers.data(),
            static_cast<uint32_t>(vk_image_barriers.size()),
            vk_image_barriers.data()
        };

        command_buffer.pipelineBarrier2KHR(dependency_desc, VulkanContext->GetDispatcher());
    }
    
    void CommandBuffer::DeferLayoutRestore(Vulkan::Image *image) {
        // Color/Depth attachments are handled in BeginRendering
        if (const auto usage_flags = image->GetDesc().usage_flags;
            !(usage_flags.Has(ImageUsageBits::eReadonlyResource) || usage_flags.Has(ImageUsageBits::eWritebleResource))) return;

        m_pending_layout_restore_images.emplace(reinterpret_cast<Vulkan::Image *>(image));
    }

    void CommandBuffer::BeginRenderingDefaultVk(const BeginRenderingDesc& desc) {
        auto* typed_pipeline = static_cast<Vulkan::GraphicsPipelineDefaultVk *>(desc.pipeline);
        const auto [vk_renderpass, vk_pipeline] = typed_pipeline->GetOrCreateAttachmentOpVariant(
                                                                        desc.attachment_ops, !desc.framebuffer);

        BarrierForPipeline(typed_pipeline->GetPipelineStageFlags(), typed_pipeline->GetAccessFlags());

        command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, 
                        typed_pipeline->GetPipelineLayout(),
                        0,
                        typed_pipeline->GetDstSets(),
                        {});

        command_buffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, 
            vk_pipeline
        );

        const auto clear_values = GetClearValues(desc);

        vk::RenderPassBeginInfo begin_desc{};
        begin_desc.setRenderPass(vk_renderpass)
                    .setClearValues(clear_values)
                    ;

        if (desc.framebuffer != nullptr) {
            auto* typed_framebuffer = static_cast<Vulkan::FramebufferDefaultVk *>(desc.framebuffer);
            begin_desc.setFramebuffer(typed_framebuffer->GetFramebuffer(vk_renderpass))
                    .setRenderArea({{}, {typed_framebuffer->GetDesc().extent.x, typed_framebuffer->GetDesc().extent.y}})
                    ;

            std::vector<Vulkan::ImageBarrier> image_barriers;
            image_barriers.reserve(typed_framebuffer->GetUserColorAttachments().size() + bool(typed_framebuffer->GetUserDepthStencilAttachment()));

            TranslateAttachmentLayouts(*typed_framebuffer);            
        }
        else {
            const auto swapchain_extent = VulkanContext->GetSwapchainProperties().extent;
            begin_desc.setFramebuffer(VulkanContext->GetOrCreateFramebuffer(vk_renderpass))
                    .setRenderArea({{}, swapchain_extent})
                    ;
        }

        command_buffer.beginRenderPass(
            begin_desc, 
            vk::SubpassContents::eInline);
    }

    void CommandBuffer::BeginRenderingDynamicRendering(const BeginRenderingDesc& desc) {
        auto* typed_pipeline = static_cast<Vulkan::GraphicsPipelineDynamicRendering *>(desc.pipeline);
        const auto vk_pipeline = typed_pipeline->GetPipeline();
        BarrierForPipeline(typed_pipeline->GetPipelineStageFlags(), typed_pipeline->GetAccessFlags());

        command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, 
                        typed_pipeline->GetPipelineLayout(),
                        0,
                        typed_pipeline->GetDstSets(),
                        {});

        command_buffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, 
            vk_pipeline
        );

        auto *typed_framebuffer = static_cast<Vulkan::FramebufferDynamicRendering *>(desc.framebuffer);
        vk::RenderingInfo rendering_info;
        rendering_info.setLayerCount(1);

        if (desc.framebuffer != nullptr) {
            std::vector<vk::RenderingAttachmentInfo> color_attachments(typed_pipeline->ColorAttachmentCount() * (1 + typed_pipeline->HasMsaa()));
            vk::RenderingAttachmentInfo depth_attachment{};
            vk::RenderingAttachmentInfo stencil_attachment{};

            if (typed_pipeline->HasMsaa()) {
                uint32_t i{};
                for (auto& color_attachment : color_attachments) {
                    color_attachment.setLoadOp(ToVk(desc.attachment_ops.color_load[i]))
                                    .setStoreOp(ToVk(desc.attachment_ops.color_store[i]))
                                    .setResolveImageView(typed_framebuffer->GetAttachments()[i])
                                    .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                    .setImageView(typed_framebuffer->GetAttachments()[typed_pipeline->ColorAttachmentCount() + i])
                                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                    .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                                    ;

                    color_attachment.setClearValue(vk::ClearColorValue(std::array{
                        desc.color_clear_values[i].r, 
                        desc.color_clear_values[i].g,
                        desc.color_clear_values[i].b, 
                        desc.color_clear_values[i].a}));
                    i++;
                }
            }
            else {
                uint32_t i{};
                for (auto& color_attachment : color_attachments) {
                    color_attachment.setLoadOp(ToVk(desc.attachment_ops.color_load[i]))
                                    .setStoreOp(ToVk(desc.attachment_ops.color_store[i]))
                                    .setImageView(typed_framebuffer->GetAttachments()[i])
                                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                    ;

                    color_attachment.setClearValue(vk::ClearColorValue(std::array{
                        desc.color_clear_values[i].r, 
                        desc.color_clear_values[i].g,
                        desc.color_clear_values[i].b, 
                        desc.color_clear_values[i].a}));
                    i++;
                }
            }

            rendering_info.setColorAttachments(color_attachments);

            if (typed_framebuffer->HasDepthAttachment()) {
                depth_attachment.setLoadOp(ToVk(desc.attachment_ops.depth_load))
                                .setStoreOp(ToVk(desc.attachment_ops.depth_store))
                                .setImageView(typed_framebuffer->GetAttachments().back())
                                .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                ;
                if (desc.attachment_ops.depth_load == AttachmentLoadOp::eClear) 
                    depth_attachment.setClearValue(vk::ClearDepthStencilValue(desc.depth_stencil_clear_value.depth, desc.depth_stencil_clear_value.stencil));

                rendering_info.setPDepthAttachment(&depth_attachment);
            }
            if (typed_framebuffer->HasStencilAttachment()) {
                depth_attachment.setLoadOp(ToVk(desc.attachment_ops.stencil_load))
                                .setStoreOp(ToVk(desc.attachment_ops.stencil_store))
                                .setImageView(typed_framebuffer->GetAttachments().back())
                                .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                ;
                if (desc.attachment_ops.stencil_load == AttachmentLoadOp::eClear) 
                    depth_attachment.setClearValue(vk::ClearDepthStencilValue(desc.depth_stencil_clear_value.depth, desc.depth_stencil_clear_value.stencil));

                rendering_info.setPStencilAttachment(&stencil_attachment);
            }

            rendering_info.setRenderArea({{}, {typed_framebuffer->GetDesc().extent.x, typed_framebuffer->GetDesc().extent.y}});
            
            TranslateAttachmentLayouts(*typed_framebuffer);

            command_buffer.beginRenderingKHR(rendering_info, VulkanContext->GetDispatcher());
        }
        else {
            TranslateSwapchainImageLayoutsInBegin();

            vk::RenderingAttachmentInfo color_attachment{};
            vk::RenderingAttachmentInfo depth_attachment{};

            if (typed_pipeline->HasMsaa()) {
                color_attachment.setLoadOp(ToVk(desc.attachment_ops.color_load[0]))
                                .setStoreOp(ToVk(desc.attachment_ops.color_store[0]))
                                .setImageView(VulkanContext->GetResolveImage()->CreateGetImageView({}))
                                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                .setResolveImageView(VulkanContext->GetSwapchainImageViews()[VulkanContext->GetImageIndex()])
                                .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                                ;   
            }
            else {
                color_attachment.setLoadOp(ToVk(desc.attachment_ops.color_load[0]))
                                .setStoreOp(ToVk(desc.attachment_ops.color_store[0]))
                                .setImageView(VulkanContext->GetSwapchainImageViews()[VulkanContext->GetImageIndex()])
                                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                ;
            }

            if (desc.attachment_ops.color_load[0] == AttachmentLoadOp::eClear) 
                color_attachment.setClearValue(vk::ClearColorValue(std::array{
                    desc.color_clear_values[0].r, 
                    desc.color_clear_values[0].g,
                    desc.color_clear_values[0].b, 
                    desc.color_clear_values[0].a}));

            depth_attachment.setLoadOp(ToVk(desc.attachment_ops.depth_load))
                            .setStoreOp(ToVk(desc.attachment_ops.depth_store))
                            .setImageView(VulkanContext->GetDepthBuffer()->CreateGetImageView({}))
                            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                            ;

            if (desc.attachment_ops.depth_load == AttachmentLoadOp::eClear) 
                depth_attachment.setClearValue(vk::ClearDepthStencilValue(desc.depth_stencil_clear_value.depth, desc.depth_stencil_clear_value.stencil));

            rendering_info.setColorAttachments(color_attachment);
            rendering_info.setPDepthAttachment(&depth_attachment);
            rendering_info.setRenderArea({{}, VulkanContext->GetSwapchainProperties().extent});

            command_buffer.beginRenderingKHR(rendering_info, VulkanContext->GetDispatcher());
        }
    }

    void CommandBuffer::TranslateAttachmentLayouts(FramebufferBase &framebuffer) {
        std::vector<Vulkan::ImageBarrier> image_barriers;
        image_barriers.reserve(framebuffer.GetUserColorAttachments().size() + bool(framebuffer.GetUserDepthStencilAttachment()));

        for (auto* image : framebuffer.GetUserColorAttachments()) {
            if (image->GetImageLayout() == vk::ImageLayout::eColorAttachmentOptimal) continue;
            image_barriers.emplace_back(
                image,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlags{},
                vk::AccessFlagBits::eColorAttachmentWrite
            );
        }
        if (auto* depth_buffer = framebuffer.GetUserDepthStencilAttachment();
            depth_buffer && depth_buffer->GetImageLayout() != vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            image_barriers.emplace_back(
                framebuffer.GetUserDepthStencilAttachment(),
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::PipelineStageFlagBits::eEarlyFragmentTests,
                vk::AccessFlags{},
                vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead
            );
        }

        if (!image_barriers.empty()) Barrier({}, image_barriers);            
    }

    void CommandBuffer::TranslateSwapchainImageLayoutsInBegin() {
        const TransferImageLayoutNativeDesc layout_transfer_desc[1] {
            {
                VulkanContext->GetSwapchainImages()[VulkanContext->GetImageIndex()],
                vk::ImageAspectFlagBits::eColor,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlags{},
                vk::AccessFlagBits::eColorAttachmentWrite
            }
        };

        TransferImageLayout(layout_transfer_desc);
    }

    void CommandBuffer::TranslateSwapchainImageLayoutsInEnd() {
        const TransferImageLayoutNativeDesc layout_transfer_desc[1] {
            {
                VulkanContext->GetSwapchainImages()[VulkanContext->GetImageIndex()],
                vk::ImageAspectFlagBits::eColor,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::AccessFlagBits::eColorAttachmentWrite,
                vk::AccessFlagBits{}
            }
        };

        TransferImageLayout(layout_transfer_desc);
    }
}