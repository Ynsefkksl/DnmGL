#pragma once

#include "DnmGL/D3D12/Context.hpp"
#include "DnmGL/D3D12/Image.hpp"

namespace DnmGL::D3D12 {
    class Framebuffer final : public DnmGL::Framebuffer {
    public:
        Framebuffer(D3D12::Context& context, const DnmGL::FramebufferDesc& desc) noexcept;
        ~Framebuffer() noexcept;

        void ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) override;

        [[nodiscard]] ID3D12DescriptorHeap *GetRtvHeap() const noexcept { return m_rtv_heap.Get(); }
        [[nodiscard]] ID3D12DescriptorHeap *GetDsvHeap() const noexcept { return m_rtv_heap.Get(); }

        [[nodiscard]] auto GetUserColorAttachments() const noexcept { return std::span(m_user_color_attachments); }
        [[nodiscard]] D3D12::Image * GetUserDepthAttachment() const noexcept { return m_user_depth_attachment; }
        [[nodiscard]] auto GetUserColorSubresourceIndex() const noexcept { return std::span(m_user_color_attachment_subresource_indices); }
        
        [[nodiscard]] auto GetMsaaColorAttachments() const noexcept { return std::span(m_msaa_color_attachments); }
        [[nodiscard]] D3D12::Image *GetDepthBuffer() const noexcept { return m_user_depth_attachment ? m_user_depth_attachment : m_depth_buffer.get(); }
        [[nodiscard]] uint32_t GetDepthSubresource() const noexcept { return m_user_depth_attachment ? m_user_depth_resource_index : 0; }
    private:
        void CreateDepthImage();
        void CreateMsaaAttachment(uint32_t attachment_index);

        ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
        ComPtr<ID3D12DescriptorHeap> m_dsv_heap;

        std::vector<D3D12::Image *> m_user_color_attachments;
        std::vector<uint32_t> m_user_color_attachment_subresource_indices;
        D3D12::Image *m_user_depth_attachment;
        uint32_t m_user_depth_resource_index;

        //for msaa
        std::vector<std::unique_ptr<D3D12::Image>> m_msaa_color_attachments{}; 
        //for utility
        std::unique_ptr<D3D12::Image> m_depth_buffer{};
    };

    inline Framebuffer::~Framebuffer() noexcept {
        D3D12Context->AddDeferDelete([
            rtv_heap = std::move(m_rtv_heap),
            dsv_heap = std::move(m_dsv_heap)
        ] () mutable {
            rtv_heap.Reset();
            dsv_heap.Reset();
        });
    }
}