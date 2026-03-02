#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class GraphicsPipeline final : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipeline(D3D12::Context& context, const DnmGL::GraphicsPipelineDesc &desc) noexcept;
        ~GraphicsPipeline() noexcept override;

        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) override;

        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;

        [[nodiscard]] auto *GetDescriptorHeap() const noexcept { return m_descriptor_heap.Get(); }
        [[nodiscard]] auto *GetSamplerHeap() const noexcept { return m_sampler_heap.Get(); }
        
        [[nodiscard]] constexpr auto ReadonlyResourceCount() const noexcept { return m_resource_counts[0]; }
        [[nodiscard]] constexpr auto WritableResourceCount() const noexcept { return m_resource_counts[1]; }
        [[nodiscard]] constexpr auto UniformResourceCount() const noexcept { return m_resource_counts[2]; }
        [[nodiscard]] constexpr auto SamplerResourceCount() const noexcept { return m_resource_counts[3]; }

        [[nodiscard]] constexpr auto *GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        // 0 readonly, 1 writable, 2 uniform, 3 sampler
        std::array<uint32_t, 4> m_resource_counts{};
        ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
        ComPtr<ID3D12DescriptorHeap> m_sampler_heap;

        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12RootSignature> m_root_signature;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline  {
    public:
        ComputePipeline(D3D12::Context& context, std::string_view shader_name) noexcept;
        ~ComputePipeline() noexcept;

        void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) override;
        void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) override;

        [[nodiscard]] constexpr auto ReadonlyResourceCount() const noexcept { return m_resource_counts[0]; }
        [[nodiscard]] constexpr auto WritableResourceCount() const noexcept { return m_resource_counts[1]; }
        [[nodiscard]] constexpr auto UniformResourceCount() const noexcept { return m_resource_counts[2]; }
        [[nodiscard]] constexpr auto SamplerResourceCount() const noexcept { return m_resource_counts[3]; }

        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;
        [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept;

        [[nodiscard]] auto *GetDescriptorHeap() const noexcept { return m_descriptor_heap.Get(); }
        [[nodiscard]] auto *GetSamplerHeap() const noexcept { return m_sampler_heap.Get(); }

        [[nodiscard]] auto * GetPipelineState(const std::string &name) const noexcept { 
            auto it = m_pipeline_states.find(name);
            return it == m_pipeline_states.end() ? nullptr : it->second.Get();
        }
        
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        // 0 readonly, 1 writable, 2 uniform, 3 sampler
        std::array<uint32_t, 4> m_resource_counts;
        ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
        ComPtr<ID3D12DescriptorHeap> m_sampler_heap;
        std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_pipeline_states;
        ComPtr<ID3D12RootSignature> m_root_signature;
    };

    inline GraphicsPipeline::~GraphicsPipeline() noexcept {
        D3D12Context->AddDeferDelete([
            pipeline_state = std::move(m_pipeline_state),
            root_signature = std::move(m_root_signature)
        ] () mutable {
            pipeline_state.Reset();
            root_signature.Reset();
        });
    }

    inline ComputePipeline::~ComputePipeline() noexcept {
        D3D12Context->AddDeferDelete([
            pipeline_states = std::move(m_pipeline_states),
            root_signature = std::move(m_root_signature)
        ] () mutable {
            for (auto [_, pipeline_state] : pipeline_states) {
                pipeline_state.Reset();
            }
            root_signature.Reset();
        });
    }

    //TODO: could be smarter
    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GraphicsPipeline::GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GraphicsPipeline::GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(
            ReadonlyResourceCount() + i + element,
            D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GraphicsPipeline::GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(ReadonlyResourceCount() +
                            WritableResourceCount() + i + element,
                        D3D12Context->GetCBV_SRV_UAVDescriptorSize());
      return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GraphicsPipeline::GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_sampler_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetSamplerDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ComputePipeline::GetReadonlyResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ComputePipeline::GetWriteableResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(
            ReadonlyResourceCount() + i + element,
            D3D12Context->GetCBV_SRV_UAVDescriptorSize());
        return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ComputePipeline::GetUniformResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(ReadonlyResourceCount() +
                            WritableResourceCount() + i + element,
                        D3D12Context->GetCBV_SRV_UAVDescriptorSize());
      return handle;
    }

    inline CD3DX12_CPU_DESCRIPTOR_HANDLE ComputePipeline::GetSamplerResourceHeapCpuHandle(uint32_t i, uint32_t element) const noexcept {
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_sampler_heap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i + element, D3D12Context->GetSamplerDescriptorSize());
        return handle;
    }
}