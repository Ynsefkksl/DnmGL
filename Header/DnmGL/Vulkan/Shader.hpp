#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    class Shader final : public DnmGL::Shader {
    public:
        Shader(Vulkan::Context& context, std::string_view filename);
        ~Shader() {
            const auto shader_module = m_shader_module;
            VulkanContext->DeleteObject(
                [shader_module] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) -> void {
                    device.destroy(shader_module);
                });
        }

        [[nodiscard]] vk::ShaderModule GetShaderModule() const { return m_shader_module; }
    private:
        vk::ShaderModule m_shader_module{ VK_NULL_HANDLE };
    };
}