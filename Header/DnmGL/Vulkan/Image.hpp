#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include <map>

namespace DnmGL::Vulkan {
    class Image final : public DnmGL::Image {
    public:
        Image(Vulkan::Context& context, const DnmGL::ImageDesc& desc);
        ~Image();

        [[nodiscard]] auto GetImage() const { return m_image; }
        [[nodiscard]] auto GetImageLayout() const { return m_image_layout; }
        [[nodiscard]] auto GetAspect() const { return m_aspect; }
        [[nodiscard]] auto *GetAllocation() const { return m_allocation; }

        [[nodiscard]] auto GetIdealImageLayout() const { return Vulkan::GetIdealImageLayout(m_desc.usage_flags); }
        [[nodiscard]] vk::ImageView CreateGetImageView(const ImageSubresource& subresource);

        vk::PipelineStageFlags prev_pipeline_stage{};
        vk::AccessFlags prev_access{};
    private:
        vk::Image m_image;
        vk::ImageLayout m_image_layout = vk::ImageLayout::ePreinitialized;
        vk::ImageAspectFlags m_aspect;
        VmaAllocation m_allocation;

        std::map<ImageSubresource, vk::ImageView> m_image_views;
        friend Vulkan::CommandBuffer;
    };
}