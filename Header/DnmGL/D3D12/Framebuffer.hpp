#pragma once

#include "DnmGL/D3D12/Context.hpp"
#include "DnmGL/D3D12/Image.hpp"

namespace DnmGL::D3D12 {
    class Framebuffer final : public DnmGL::Framebuffer {
    public:
        Framebuffer(D3D12::Context& context, const DnmGL::FramebufferDesc& desc) noexcept
        : DnmGL::Framebuffer(context, desc) {}
        ~Framebuffer() noexcept {}

        void ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) override {}
    private:
    };
}