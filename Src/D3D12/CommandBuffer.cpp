#include "DnmGL/D3D12/CommandBuffer.hpp"
#include "DnmGL/D3D12/Pipeline.hpp"
#include "DnmGL/D3D12/Buffer.hpp"
#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/ToDxgiFormat.hpp"

namespace DnmGL::D3D12 {
    CommandBuffer::CommandBuffer(D3D12::Context& ctx)
        : DnmGL::CommandBuffer(ctx) {
        D3D12Context->GetDevice()->CreateCommandList(
            0, 
            D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, 
            D3D12Context->GetCommandAllocator(), 
            nullptr, 
            __uuidof(ID3D12GraphicsCommandList), 
            &m_command_list);
    }

    void CommandBuffer::IBindPipeline(const DnmGL::ComputePipeline* pipeline) {
        DnmGLAssert(false, "this func is not complate")
        //m_command_list->SetComputeRoot

        const auto *typed_pipeline = static_cast<const D3D12::ComputePipeline *>(pipeline);
        m_command_list->SetPipelineState(typed_pipeline->GetPipelineState());
    }

    void CommandBuffer::ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) {
        auto *typed_buffer = static_cast<D3D12::Buffer *>(desc.dst_buffer);
        auto *typed_image = static_cast<D3D12::Image *>(desc.src_image);

        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = typed_buffer->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = D3D12CalcSubresource(
                desc.image_subresource.base_mipmap, 
                desc.image_subresource.base_layer, 
                0,
                desc.src_image->GetDesc().mipmap_levels,
                desc.src_image->GetDesc().type == ImageType::e2D ? desc.src_image->GetDesc().extent.z : 1)
        };

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

        const auto res_desc = typed_image->GetResource()->GetDesc();

        D3D12Context->GetDevice()->GetCopyableFootprints(
            &res_desc, 
            0, 
            1, 
            desc.buffer_offset, 
            &footprint,
            nullptr,
            nullptr,
            nullptr);

        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = typed_image->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = footprint
        };

        const D3D12_BOX box {
            .left   = desc.copy_offset.x,
            .top    = desc.copy_offset.y,
            .front  = desc.copy_offset.z,
            .right  = desc.copy_extent.x,
            .bottom = desc.copy_extent.y,
            .back   = desc.copy_extent.z
        };

        {
            const auto need_src_barrier = typed_image->GetState() != D3D12_RESOURCE_STATE_COPY_SOURCE;
            const auto need_dst_barrier = typed_buffer->GetState() != D3D12_RESOURCE_STATE_COPY_DEST;
    
            D3D12_RESOURCE_BARRIER barrier[need_src_barrier + need_dst_barrier];
            if (need_src_barrier) {
                barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[0].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_buffer->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_buffer->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
                };
                barrier[0].Flags = {};
                typed_buffer->m_state = D3D12_RESOURCE_STATE_COPY_DEST;
            }
            if (need_dst_barrier) {
                const auto index = need_src_barrier;
                barrier[index].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[index].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_image->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_image->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE,
                };
                barrier[index].Flags = {};
                typed_image->m_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            }
    
            m_command_list->ResourceBarrier(need_src_barrier + need_dst_barrier, barrier);
        }

        m_command_list->CopyTextureRegion(
            &src, 0, 0, 0, 
            &src, &box);
    }

    void CommandBuffer::ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) {
        auto *typed_src_image = static_cast<D3D12::Image *>(desc.src_image);
        auto *typed_dst_image = static_cast<D3D12::Image *>(desc.dst_image);

        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = typed_dst_image->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = D3D12CalcSubresource(
                desc.dst_image_subresource.base_mipmap, 
                desc.dst_image_subresource.base_layer, 
                0,
                desc.dst_image->GetDesc().mipmap_levels,
                desc.dst_image->GetDesc().type == ImageType::e2D ? desc.dst_image->GetDesc().extent.z : 1)
        };

        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = typed_src_image->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = D3D12CalcSubresource(
                desc.src_image_subresource.base_mipmap, 
                desc.src_image_subresource.base_layer, 
                0,
                desc.dst_image->GetDesc().mipmap_levels,
                desc.dst_image->GetDesc().type == ImageType::e2D ? desc.dst_image->GetDesc().extent.z : 1)
        };

        const D3D12_BOX box {
            .left   = desc.src_offset.x,
            .top    = desc.src_offset.y,
            .front  = desc.src_offset.z,
            .right  = desc.copy_extent.x,
            .bottom = desc.copy_extent.y,
            .back   = desc.copy_extent.z
        };

        {
            const auto need_src_barrier = typed_src_image->GetState() != D3D12_RESOURCE_STATE_COPY_SOURCE;
            const auto need_dst_barrier = typed_dst_image->GetState() != D3D12_RESOURCE_STATE_COPY_DEST;
    
            D3D12_RESOURCE_BARRIER barrier[need_src_barrier + need_dst_barrier];
            if (need_src_barrier) {
                barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[0].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_src_image->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_src_image->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE,
                };
                barrier[0].Flags = {};
                typed_src_image->m_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            }
            if (need_dst_barrier) {
                const auto index = need_src_barrier;
                barrier[index].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[index].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_dst_image->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_dst_image->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
                };
                barrier[index].Flags = {};
                typed_dst_image->m_state = D3D12_RESOURCE_STATE_COPY_DEST;
            }
    
            m_command_list->ResourceBarrier(need_src_barrier + need_dst_barrier, barrier);
        }

        m_command_list->CopyTextureRegion(
            &dst, desc.dst_offset.x, desc.dst_offset.y, desc.dst_offset.z, 
            &src, &box);
    }

    void CommandBuffer::ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) {
        auto *typed_image = static_cast<D3D12::Image *>(desc.dst_image);
        auto *typed_buffer = static_cast<D3D12::Buffer *>(desc.src_buffer);

        const auto subresource_index = D3D12CalcSubresource(
                desc.image_subresource.base_mipmap, 
                desc.image_subresource.base_layer, 
                0,
                desc.dst_image->GetDesc().mipmap_levels,
                desc.dst_image->GetDesc().type == ImageType::e2D ? desc.dst_image->GetDesc().extent.z : 1);

        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = typed_image->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = subresource_index
        };

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        footprint.Offset = 0;
        footprint.Footprint.Format = typed_image->GetResource()->GetDesc().Format;
        footprint.Footprint.Width = desc.copy_extent.x;
        footprint.Footprint.Height = desc.copy_extent.y;
        footprint.Footprint.Depth  = desc.copy_extent.z;
        footprint.Footprint.RowPitch =
            D3DX12Align(desc.copy_extent.x * GetFormatSize(typed_image->GetDesc().format), 128u);

        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = typed_buffer->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = footprint
        };

        const D3D12_BOX box {
            .left   = {},
            .top    = {},
            .front  = {},
            .right  = desc.copy_extent.x,
            .bottom = desc.copy_extent.y,
            .back   = desc.copy_extent.z
        };

        {
            const auto need_src_barrier = typed_buffer->GetState() != D3D12_RESOURCE_STATE_COPY_SOURCE;
            const auto need_dst_barrier = typed_image->GetState() != D3D12_RESOURCE_STATE_COPY_DEST;
    
            D3D12_RESOURCE_BARRIER barrier[need_src_barrier + need_dst_barrier];
            if (need_src_barrier) {
                barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[0].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_buffer->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_buffer->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE,
                };
                barrier[0].Flags = {};
                typed_buffer->m_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            }
            if (need_dst_barrier) {
                const auto index = need_src_barrier;
                barrier[index].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[index].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_image->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_image->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
                };
                barrier[index].Flags = {};
                typed_image->m_state = D3D12_RESOURCE_STATE_COPY_DEST;
            }
    
            m_command_list->ResourceBarrier(need_src_barrier + need_dst_barrier, barrier);
        }

        m_command_list->CopyTextureRegion(
            &dst, desc.copy_offset.x, desc.copy_offset.y, desc.copy_offset.z, 
            &src, &box);
    }

    void CommandBuffer::ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) {
        auto *typed_src_buffer = static_cast<D3D12::Buffer *>(desc.src_buffer);
        auto *typed_dst_buffer = static_cast<D3D12::Buffer *>(desc.dst_buffer);

        {
            const auto need_src_barrier = typed_src_buffer->GetState() != D3D12_RESOURCE_STATE_COPY_SOURCE;
            const auto need_dst_barrier = typed_dst_buffer->GetState() != D3D12_RESOURCE_STATE_COPY_DEST;
    
            D3D12_RESOURCE_BARRIER barrier[need_src_barrier + need_dst_barrier];
            if (need_src_barrier) {
                barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[0].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_src_buffer->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_src_buffer->GetIdealState(),
                    .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
                };
                barrier[0].Flags = {};
                typed_src_buffer->m_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            }
            if (need_dst_barrier) {
                const auto index = need_src_barrier;
                barrier[index].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier[index].Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = typed_dst_buffer->GetResource(),
                    .Subresource = 0,
                    .StateBefore = typed_dst_buffer->GetState(),
                    .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
                };
                barrier[index].Flags = {};
                typed_dst_buffer->m_state = D3D12_RESOURCE_STATE_COPY_DEST;
            }
    
            m_command_list->ResourceBarrier(need_src_barrier + need_dst_barrier, barrier);
        }

        m_command_list->CopyBufferRegion(
            typed_dst_buffer->GetResource(), desc.dst_offset, 
            typed_src_buffer->GetResource(), desc.src_offset, desc.copy_size);
    }

    void CommandBuffer::IUploadData(DnmGL::Image *image, 
                        const ImageSubresource& subresource, 
                        const void* data, 
                        Uint3 copy_extent, 
                        Uint3 copy_offset) {

        const auto copy_size = copy_extent.x * copy_extent.y * copy_extent.z * GetFormatSize(image->GetDesc().format);

        auto *typed_image = static_cast<D3D12::Image *>(image);
        auto *dst_resource = typed_image->GetResource();

        const auto subresource_index = D3D12CalcSubresource(
                subresource.base_mipmap, 
                subresource.base_layer, 
                0,
                image->GetDesc().mipmap_levels,
                image->GetDesc().type == ImageType::e2D ? image->GetDesc().extent.z : 1);

        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = dst_resource,
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = subresource_index
        };

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};

        const auto res_desc = dst_resource->GetDesc();
        uint64_t staging_buffer_size;

        D3D12Context->GetDevice()->GetCopyableFootprints(
            &res_desc, 
            subresource_index,
            1, 
            0,
            &footprint,
            nullptr,
            nullptr,
            &staging_buffer_size);
            
        D3D12::Buffer staging_buffer(*D3D12Context, {
            .element_size = static_cast<uint32_t>(staging_buffer_size),
            .element_count = 1,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .usage_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr(), data, copy_size); 

        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = staging_buffer.GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = footprint
        };

        const D3D12_BOX box {
            .left   = copy_offset.x,
            .top    = copy_offset.y,
            .front  = copy_offset.z,
            .right  = copy_extent.x,
            .bottom = copy_extent.y,
            .back   = copy_extent.z
        };

        if (typed_image->GetState() != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                .pResource = typed_image->GetResource(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = typed_image->GetState(),
                .StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
            };
            m_command_list->ResourceBarrier(1, &barrier);
            typed_image->m_state = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;
        }

        m_command_list->CopyTextureRegion(
            &dst, 0, 0, 0, 
            &src, &box);
    }

    void CommandBuffer::IUploadData(DnmGL::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) {
        auto *typed_buffer = static_cast<D3D12::Buffer *>(buffer);

        D3D12::Buffer staging_buffer(*D3D12Context, {
            .element_size = size,
            .element_count = 1,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .usage_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr(), data, size);

        CopyBufferToBuffer({
            .src_buffer = &staging_buffer,
            .dst_buffer = typed_buffer,
            .src_offset = offset,
            .dst_offset = 0,
            .copy_size = size,
        });
    }
}