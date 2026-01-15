#pragma once

#include "DnmGL/Vulkan/Context.hpp"
#include "DnmGL/Vulkan/Shader.hpp"

namespace DnmGL::Vulkan {
    struct EntryPointInfo;

    class ResourceManager final : public DnmGL::ResourceManager {
    public:
        ResourceManager(DnmGL::Vulkan::Context& context, std::span<const DnmGL::Shader *> shaders);
        ~ResourceManager();

        void ISetReadonlyResource(std::span<const ResourceDesc> update_resource) override;
        void ISetWritableResource(std::span<const ResourceDesc> update_resource) override;
        void ISetUniformBuffer(std::span<const UniformResourceDesc> update_resource) override;
        void ISetSampler(std::span<const SamplerResourceDesc> update_resource) override;

        [[nodiscard]] std::span<const vk::DescriptorSet, 4> GetDescriptorSets() const noexcept { return m_dst_sets; }
        [[nodiscard]] std::span<const vk::DescriptorSetLayout, 4> GetDescriptorLayouts() const noexcept { return m_dst_set_layouts; }

        [[nodiscard]] vk::DescriptorSet GetReadonlySet() const noexcept { return m_dst_sets[0]; }
        [[nodiscard]] vk::DescriptorSetLayout GetReadonlySetLayout() const noexcept { return m_dst_set_layouts[0]; }

        [[nodiscard]] vk::DescriptorSet GetWritableSet() const noexcept { return m_dst_sets[1]; }
        [[nodiscard]] vk::DescriptorSetLayout GetWritableSetLayout() const noexcept { return m_dst_set_layouts[1]; }

        [[nodiscard]] vk::DescriptorSet GetUniformSet() const noexcept { return m_dst_sets[2]; }
        [[nodiscard]] vk::DescriptorSetLayout GetUniformSetLayout() const noexcept { return m_dst_set_layouts[2]; }

        [[nodiscard]] vk::DescriptorSet GetSamplerSet() const noexcept { return m_dst_sets[3]; }
        [[nodiscard]] vk::DescriptorSetLayout GetSamplerSetLayout() const noexcept { return m_dst_set_layouts[3]; }

        void FillDescriptorSets(std::span<vk::DescriptorSet, 4> sets, std::span<const VkEntryPointInfo *> entry_points) const noexcept;
        void FillDescriptorSetLayouts(std::span<vk::DescriptorSetLayout, 4> layouts, std::span<const VkEntryPointInfo *> entry_points) const noexcept;
    private:
        std::array<vk::DescriptorSet, 4> m_dst_sets;
        std::array<vk::DescriptorSetLayout, 4> m_dst_set_layouts;
    };

    inline ResourceManager::~ResourceManager() {
        VulkanContext->DeleteObject(
            [
                layouts = m_dst_set_layouts
            ] (vk::Device device, [[maybe_unused]] VmaAllocator) -> void {
                for (const auto layout : layouts) {
                    device.destroy(layout);
                }
            });
    }

    inline void ResourceManager::FillDescriptorSets(std::span<vk::DescriptorSet, 4> sets, std::span<const VkEntryPointInfo *> entry_points) const noexcept {
        const auto empty_set = VulkanContext->GetEmptySet();

        bool has_readonly_res{};
        bool has_writable_res{};
        bool has_uniform_res{};
        bool has_sampler_res{};

        for (const auto *entry_point : entry_points) {
            has_readonly_res |= entry_point->HasReadonlyResource();
            has_writable_res |= entry_point->HasWritableResource();
            has_uniform_res |= entry_point->HasUniformResource();
            has_sampler_res |= entry_point->HasSamplerResource();
        }

        sets[0] = has_readonly_res ? GetReadonlySet() : empty_set;
        sets[1] = has_writable_res ? GetWritableSet() : empty_set;
        sets[2] = has_uniform_res ? GetUniformSet() : empty_set;
        sets[3] = has_sampler_res ? GetSamplerSet() : empty_set;
    }

    inline void ResourceManager::FillDescriptorSetLayouts(std::span<vk::DescriptorSetLayout, 4> layouts, std::span<const VkEntryPointInfo *> entry_points) const noexcept {
        const auto empty_set = VulkanContext->GetEmptySetLayout();

        bool has_readonly_res{};
        bool has_writable_res{};
        bool has_uniform_res{};
        bool has_sampler_res{};

        for (const auto *entry_point : entry_points) {
            has_readonly_res |= entry_point->HasReadonlyResource();
            has_writable_res |= entry_point->HasWritableResource();
            has_uniform_res |= entry_point->HasUniformResource();
            has_sampler_res |= entry_point->HasSamplerResource();
        }

        layouts[0] = has_readonly_res ? GetReadonlySetLayout() : empty_set;
        layouts[1] = has_writable_res ? GetWritableSetLayout() : empty_set;
        layouts[2] = has_uniform_res ? GetUniformSetLayout() : empty_set;
        layouts[3] = has_sampler_res ? GetSamplerSetLayout() : empty_set;
    }
}