#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class GraphicsPipeline final : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipeline(D3D12::Context& context, const DnmGL::GraphicsPipelineDesc& desc) noexcept;
        ~GraphicsPipeline() = default;

        [[nodiscard]] constexpr auto *GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12RootSignature> m_root_signature;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline  {
    public:
        ComputePipeline(D3D12::Context& context, const DnmGL::ComputePipelineDesc& desc) noexcept;
        ~ComputePipeline() = default;

        [[nodiscard]] constexpr auto *GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12RootSignature> m_root_signature;
    };
}