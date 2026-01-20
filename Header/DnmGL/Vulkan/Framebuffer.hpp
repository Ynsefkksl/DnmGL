#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include "DnmGL/Vulkan/Image.hpp"

namespace DnmGL::Vulkan {
    //TODO: get dynamic rendering
    //TODO: get imageless framebuffer

    class FramebufferBase : public DnmGL::Framebuffer {
    public:
        FramebufferBase(Vulkan::Context& context, const DnmGL::FramebufferDesc& desc) noexcept;

        [[nodiscard]] constexpr std::span<Vulkan::Image *> GetUserColorAttachments() noexcept { return m_user_color_attachments; }
        [[nodiscard]] constexpr Vulkan::Image * GetUserDepthStencilAttachment() noexcept { return m_user_depth_stencil_attachment; }

        [[nodiscard]] constexpr std::span<const vk::ImageView> GetAttachments() const noexcept { return m_attachments; }
    protected:
        std::unique_ptr<Vulkan::Image> CreateResource(Uint2 extent, ImageUsageFlags usage, DnmGL::ImageFormat format, SampleCount sample_count) noexcept;
        void SetAttachment(std::span<const DnmGL::RenderAttachment> color_attachments, 
                            DnmGL::RenderAttachment depth_stencil_attachment);
        std::vector<vk::ImageView> m_attachments;

        //for sync
        std::vector<Vulkan::Image *> m_user_color_attachments{}; 
        Vulkan::Image *m_user_depth_stencil_attachment{};

        //for msaa
        std::vector<std::unique_ptr<Vulkan::Image>> m_msaa_color_attachments{}; 
        //for utility
        std::unique_ptr<Vulkan::Image> m_depth_buffer{};
    };

    class FramebufferDefaultVk : public FramebufferBase {
    public:
        FramebufferDefaultVk(Vulkan::Context& context, const DnmGL::FramebufferDesc& desc) noexcept;
        ~FramebufferDefaultVk() noexcept;

        void ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) override;

        vk::Framebuffer GetFramebuffer(vk::RenderPass renderpass);
    protected:
        void DeleteFramebuffers(std::vector<vk::Framebuffer>&& framebuffers) const noexcept;

        //vk::RenderPass cannot be hashed, but VkRenderPass can
        std::unordered_map<VkRenderPass, vk::Framebuffer> m_framebuffers;
    private:
        vk::Framebuffer CreateFramebuffer(vk::RenderPass renderpass);
    };

    class FramebufferDynamicRendering final : public FramebufferBase {
    public:
        FramebufferDynamicRendering(Vulkan::Context& context, const DnmGL::FramebufferDesc& desc) noexcept;
        ~FramebufferDynamicRendering() noexcept {}

        void ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) override;
    };

    inline void FramebufferDefaultVk::DeleteFramebuffers(std::vector<vk::Framebuffer>&& framebuffers) const noexcept {
        VulkanContext->DeleteObject(
            [
                framebuffers = std::move(framebuffers)
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto framebuffer : framebuffers) {
                    device.destroy(framebuffer);
                }
            });
    }
    
    inline FramebufferDefaultVk::FramebufferDefaultVk(Vulkan::Context& ctx, const DnmGL::FramebufferDesc& desc) noexcept 
    : Vulkan::FramebufferBase(ctx, desc) {}
    
    inline FramebufferDefaultVk::~FramebufferDefaultVk() noexcept {
        VulkanContext->DeleteObject(
            [
                framebuffers = std::move(m_framebuffers)
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) noexcept -> void {
                for (const auto [_, framebuffer] : framebuffers) {
                    device.destroy(framebuffer);
                }
        });
    }

    inline vk::Framebuffer FramebufferDefaultVk::GetFramebuffer(vk::RenderPass renderpass) {
        auto [it, is_empleced] = m_framebuffers.try_emplace(renderpass, vk::Framebuffer(VK_NULL_HANDLE));
        if (is_empleced)
            it->second = CreateFramebuffer(renderpass);
        return it->second;
    }

    inline vk::Framebuffer FramebufferDefaultVk::CreateFramebuffer(vk::RenderPass renderpass) {
        vk::FramebufferCreateInfo framebuffer_info{};
        framebuffer_info.setAttachments(m_attachments)
                        .setWidth(m_desc.extent.x)
                        .setHeight(m_desc.extent.y)
                        .setLayers(1)
                        .setRenderPass(renderpass);
                        ;

        return VulkanContext->GetDevice().createFramebuffer(framebuffer_info);
    }

    inline void FramebufferDefaultVk::ISetAttachments(
            std::span<const DnmGL::RenderAttachment> color_attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) {
        SetAttachment(color_attachments, depth_stencil_attachment);

        std::vector<vk::Framebuffer> deleted_framebuffer;
        deleted_framebuffer.reserve(color_attachments.size());
        for (const auto [renderpass, framebuffer] : m_framebuffers) {
            deleted_framebuffer.push_back(framebuffer);
            m_framebuffers[renderpass] = CreateFramebuffer(renderpass);
        }
        DeleteFramebuffers(std::move(deleted_framebuffer));
    }

    inline FramebufferDynamicRendering::FramebufferDynamicRendering(Vulkan::Context& ctx, const DnmGL::FramebufferDesc& desc) noexcept 
    : Vulkan::FramebufferBase(ctx, desc) {}
    
    inline void FramebufferDynamicRendering::ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) {
        SetAttachment(attachments, depth_stencil_attachment);
    }
}