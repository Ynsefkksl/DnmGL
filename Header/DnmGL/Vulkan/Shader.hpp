#pragma once

#include "DnmGL/Vulkan/Context.hpp"

namespace DnmGL::Vulkan {
    struct VkBindingInfo {
        uint32_t binding;
        uint32_t descriptor_count;
        vk::DescriptorType type;
    };

    struct VkEntryPointInfo {
        std::string name;
        std::vector<VkBindingInfo> readonly_resources;
        std::vector<VkBindingInfo> writable_resources;
        std::vector<VkBindingInfo> uniform_buffer_resources;
        std::vector<VkBindingInfo> sampler_resources;

        std::optional<PushConstantInfo> push_constant;
        vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eAll;

        vk::AccessFlags access_flags;

        constexpr bool HasReadonlyResource() const noexcept { return !readonly_resources.empty(); }
        constexpr bool HasWritableResource() const noexcept { return !writable_resources.empty(); }
        constexpr bool HasUniformResource() const noexcept { return !uniform_buffer_resources.empty(); }
        constexpr bool HasSamplerResource() const noexcept { return !sampler_resources.empty(); }
    };

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

        [[nodiscard]] const auto &GetVkEntryPoints() const { return m_vk_entry_points; }
        [[nodiscard]] const auto *GetVkEntryPoint(const std::string_view name) const;
    private:
        vk::ShaderModule m_shader_module{ VK_NULL_HANDLE };
        
        std::vector<VkEntryPointInfo> m_vk_entry_points;
    };

    inline const auto *Shader::GetVkEntryPoint(const std::string_view name) const {
        const auto it = std::ranges::find_if(m_vk_entry_points, [name] (const auto& entry_point) -> bool {
            return entry_point.name == name;
        });
        return it != m_vk_entry_points.end() ? &(*it) : nullptr;
    }
}