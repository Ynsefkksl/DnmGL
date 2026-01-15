#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    class Buffer final : public DnmGL::Buffer {
    public:
        Buffer(Vulkan::Context& context, const DnmGL::BufferDesc& desc);
        ~Buffer();

        [[nodiscard]] auto GetBuffer() const { return m_buffer; }
        [[nodiscard]] auto* GetAllocation() const { return m_allocation; }
        
        vk::PipelineStageFlags prev_pipeline_stage{};
        vk::AccessFlags prev_access{};
    private:
        vk::Buffer m_buffer;
        VmaAllocation m_allocation;
    };
}