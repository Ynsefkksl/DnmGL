#include "DnmGL/D3D12/Framebuffer.hpp"
#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/Shader.hpp"
#include "DnmGL/D3D12/ResourceManager.hpp"

namespace DnmGL::D3D12 {
    static constexpr D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDimention(const ImageSubresource &subresource) noexcept {
        switch (subresource.type) {
            case ImageSubresourceType::e1D: {
                return D3D12_DEPTH_STENCIL_VIEW_DESC {
                    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D,
                    .Texture1D = {
                        subresource.base_mipmap
                    }
                };
            }
            case ImageSubresourceType::e2D: {
                return D3D12_DEPTH_STENCIL_VIEW_DESC {
                    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        subresource.base_mipmap
                    }
                };
            }
            case ImageSubresourceType::e2DArray: {
                return D3D12_DEPTH_STENCIL_VIEW_DESC {
                    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY,
                    .Texture2DArray = {
                        subresource.base_mipmap,
                        subresource.base_layer,
                        subresource.layer_count
                    }
                };
            }
            default: std::unreachable();
        }
    }

    static constexpr D3D12_RENDER_TARGET_VIEW_DESC GetRtvDimention(const ImageSubresource &subresource) noexcept {
        switch (subresource.type) {
            case ImageSubresourceType::e1D: {
                return D3D12_RENDER_TARGET_VIEW_DESC {
                    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D,
                    .Texture1D = {
                        subresource.base_mipmap
                    }
                };
            }
            case ImageSubresourceType::e2D: {
                return D3D12_RENDER_TARGET_VIEW_DESC {
                    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        subresource.base_mipmap
                    }
                };
            }
            case ImageSubresourceType::e2DArray: {
                return D3D12_RENDER_TARGET_VIEW_DESC {
                    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
                    .Texture2DArray = {
                        subresource.base_mipmap,
                        subresource.base_layer,
                        subresource.layer_count
                    }
                };
            }
            default: std::unreachable();
        }   
    }
    
    Framebuffer::Framebuffer(D3D12::Context& ctx, const DnmGL::FramebufferDesc& desc) noexcept
        : DnmGL::Framebuffer(ctx, desc) {
        D3D12_DESCRIPTOR_HEAP_DESC dst_heap_desc{};
        if (ColorAttachmentCount() != 0) {
            dst_heap_desc.NumDescriptors = ColorAttachmentCount();
            dst_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

            D3D12Context->GetDevice()->CreateDescriptorHeap(
                &dst_heap_desc, 
                IID_PPV_ARGS(&m_rtv_heap));
        }

        if (HasDepthAttachment()) {
            dst_heap_desc.NumDescriptors = 1;
            dst_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

            D3D12Context->GetDevice()->CreateDescriptorHeap(
                &dst_heap_desc, 
                IID_PPV_ARGS(&m_dsv_heap));
        }

        m_user_color_attachments.resize(ColorAttachmentCount());
    }

    void Framebuffer::ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) {
        m_user_color_attachments.clear();
        m_user_depth_attachment = nullptr;

        for (const auto i : Counter(ColorAttachmentCount()))
            m_user_color_attachments[i] = static_cast<D3D12::Image *>(attachments[i].image);

        if (HasMsaa()) {
            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart());

            for (const auto i : Counter(ColorAttachmentCount())) {
                CreateMsaaAttachment(i);
                
                auto rtv_desc = GetRtvDimention({});
                rtv_desc.Format = m_msaa_color_attachments[i]->GetFormat();
                
                D3D12Context->GetDevice()->CreateRenderTargetView(
                    m_msaa_color_attachments[i]->GetResource(), 
                    &rtv_desc, 
                    handle);

                handle.Offset(1, D3D12Context->GetRTVDescriptorSize());
            }
        }
        else {
            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart());
            
            for (const auto i : Counter(ColorAttachmentCount())) {
                auto rtv_desc = GetRtvDimention({});
                rtv_desc.Format = m_user_color_attachments[i]->GetFormat();
                        
                D3D12Context->GetDevice()->CreateRenderTargetView(
                    m_user_color_attachments[i]->GetResource(), 
                    &rtv_desc, 
                    handle);

                handle.Offset(1, D3D12Context->GetRTVDescriptorSize());
            }
        }

        if (HasDepthAttachment()) {
            if (depth_stencil_attachment.image == nullptr) {
                CreateDepthImage();

                auto dsv_desc = GetDsvDimention({});
                dsv_desc.Format = m_depth_buffer->GetFormat();

                m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
                D3D12Context->GetDevice()->CreateDepthStencilView(
                    m_depth_buffer->GetResource(), 
                    &dsv_desc, 
                    m_dsv_heap->GetCPUDescriptorHandleForHeapStart());
            }
            else {
                m_user_depth_attachment = static_cast<D3D12::Image *>(depth_stencil_attachment.image);

                auto dsv_desc = GetDsvDimention(depth_stencil_attachment.subresource);
                dsv_desc.Format = m_user_depth_attachment->GetFormat();

                m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
                D3D12Context->GetDevice()->CreateDepthStencilView(
                    m_user_depth_attachment->GetResource(), 
                    &dsv_desc, 
                    m_dsv_heap->GetCPUDescriptorHandleForHeapStart());
            }
        }
    }

    void Framebuffer::CreateDepthImage() {
        m_depth_buffer = std::make_unique<D3D12::Image>(
            *D3D12Context, 
            DnmGL::ImageDesc{
                .extent = {m_desc.extent, 1},
                .format = m_desc.depth_stencil_format,
                .usage_flags = ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment,
                .type = ImageType::e2D,
                .mipmap_levels = 1,
                .sample_count = m_desc.msaa
            });
    }

    void Framebuffer::CreateMsaaAttachment(uint32_t attachment_index) {
        m_msaa_color_attachments.emplace_back(std::make_unique<D3D12::Image>(
            *D3D12Context, 
            DnmGL::ImageDesc{
                .extent = {m_desc.extent, 1},
                .format = m_desc.color_attachment_formats[attachment_index],
                .usage_flags = ImageUsageBits::eColorAttachment | ImageUsageBits::eTransientAttachment,
                .type = ImageType::e2D,
                .mipmap_levels = 1,
                .sample_count = m_desc.msaa
            }));
    }
}