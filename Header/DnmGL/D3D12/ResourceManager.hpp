#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class ResourceManager final : public DnmGL::ResourceManager {
    public:
        ResourceManager(DnmGL::D3D12::Context& context, std::span<const DnmGL::Shader *> shaders);
        ~ResourceManager() = default;

        void ISetReadonlyResource(std::span<const ResourceDesc> update_resource) override;
        void ISetWritableResource(std::span<const ResourceDesc> update_resource) override;
        void ISetUniformResource(std::span<const UniformResourceDesc> update_resource) override;
        void ISetSamplerResource(std::span<const SamplerResourceDesc> update_resource) override;

        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;

        [[nodiscard]] std::vector<D3D12_DESCRIPTOR_RANGE> GetDescriptorRangesSRV_UAV_CBV(std::span<const EntryPointInfo *> entry_point_infos) const noexcept;
        [[nodiscard]] std::optional<D3D12_DESCRIPTOR_RANGE> GetDescriptorRangeSampler(std::span<const EntryPointInfo *> entry_point_infos) const noexcept;
        
        [[nodiscard]] auto *GetDescriptorHeap() const noexcept { return m_descriptor_heap.Get(); }
        [[nodiscard]] auto *GetSamplerHeap() const noexcept { return m_sampler_heap.Get(); }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorHeapGPUHandle() const noexcept { return m_descriptor_heap->GetGPUDescriptorHandleForHeapStart(); }
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerHeapGPUHandle() const noexcept { return m_sampler_heap->GetGPUDescriptorHandleForHeapStart(); }
    private:
        ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
        ComPtr<ID3D12DescriptorHeap> m_sampler_heap;
        uint32_t m_readonly_resource_count{};
        uint32_t m_writable_resource_count{};
        uint32_t m_uniform_resource_count{};
        uint32_t m_sampler_resource_count{};
    };

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(
            m_readonly_resource_count + i + element,
            D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(m_readonly_resource_count +
                            m_writable_resource_count + i + element,
                        D3D12Context->GetCBV_SRV_UAVDescriptorSize());
      return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_sampler_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetSamplerDescriptorSize());
        return handle;
    }
}