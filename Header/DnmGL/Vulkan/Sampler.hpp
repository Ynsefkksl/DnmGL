#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    class Sampler final : public DnmGL::Sampler {
    public:
        Sampler(DnmGL::Vulkan::Context& context, const DnmGL::SamplerDesc& desc);
        ~Sampler() {
            const auto sampler = m_sampler;
            VulkanContext->DeleteObject(
                [sampler] (vk::Device device, [[maybe_unused]] VmaAllocator) -> void {
                    device.destroy(sampler);
                });
        }

        [[nodiscard]] vk::Sampler GetSampler() const { return m_sampler; }
    private:
        vk::Sampler m_sampler;
    };
}