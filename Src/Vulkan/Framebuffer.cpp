#include "DnmGL/Vulkan/Framebuffer.hpp"
#include "DnmGL/Vulkan/ToVkFormat.hpp"

namespace DnmGL::Vulkan {
    FramebufferBase::FramebufferBase(Vulkan::Context& ctx, const DnmGL::FramebufferDesc& desc) noexcept 
    : DnmGL::Framebuffer(ctx, desc) {
        m_user_color_attachments.reserve(desc.color_attachment_formats.size());

        if (m_desc.msaa != SampleCount::e1) {
            m_msaa_color_attachments.reserve(desc.color_attachment_formats.size());
        }
        
        if (m_desc.create_depth_buffer) {
            m_depth_buffer = CreateResource(
                        m_desc.extent,
                        ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment, 
                        m_desc.depth_stencil_format, 
                        m_desc.msaa);
        }
    }

    void FramebufferBase::SetAttachment(std::span<const DnmGL::RenderAttachment> color_attachments, 
                                                DnmGL::RenderAttachment depth_stencil_attachment) {
        m_user_color_attachments.resize(0);
        m_user_depth_stencil_attachment = nullptr;
        m_attachments.clear();
        
        for (const auto i : Counter(color_attachments.size())) {
            auto* typed_image = reinterpret_cast<Vulkan::Image *>(color_attachments[i].image);
            m_attachments.emplace_back(typed_image->CreateGetImageView(color_attachments[i].subresource));
            m_user_color_attachments.emplace_back(typed_image);
        }

        if (HasMsaa())
            for (const auto i : Counter(ColorAttachmentCount())) {
                auto image = CreateResource(
                        m_desc.extent, 
                        ImageUsageBits::eColorAttachment | ImageUsageBits::eTransientAttachment, 
                        m_desc.color_attachment_formats[i],
                        m_desc.msaa);

                m_attachments.emplace_back(image->CreateGetImageView(ImageSubresource{}));
                m_msaa_color_attachments.emplace_back(std::move(image));
            }

        if (HasDepthAttachment()) {
            if (depth_stencil_attachment.image != nullptr) {
                auto* typed_image = reinterpret_cast<Vulkan::Image*>(depth_stencil_attachment.image);
                m_attachments.emplace_back(typed_image->CreateGetImageView(depth_stencil_attachment.subresource));
                m_user_depth_stencil_attachment = typed_image;
            }
            else {
                if (!m_depth_buffer) {
                    m_depth_buffer = CreateResource(
                        m_desc.extent,
                        ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment, 
                        m_desc.depth_stencil_format, 
                        m_desc.msaa);
                }
                m_attachments.emplace_back(m_depth_buffer->CreateGetImageView({}));
            }
        }
    }

    std::unique_ptr<Vulkan::Image> FramebufferBase::CreateResource(Uint2 extent, ImageUsageFlags usage, DnmGL::ImageFormat format, SampleCount sample_count) noexcept {
        return std::make_unique<Vulkan::Image>(
            *VulkanContext, 
            DnmGL::ImageDesc{
                .extent = {extent.x, extent.y, 1},
                .format = format,
                .usage_flags = usage,
                .type = ImageType::e2D,
                .mipmap_levels = 1,
                .sample_count = sample_count,
            });
    }
}