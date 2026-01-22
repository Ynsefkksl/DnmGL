#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class GraphicsPipeline final : public DnmGL::GraphicsPipeline {
    public:
        GraphicsPipeline(D3D12::Context& context, const DnmGL::GraphicsPipelineDesc& desc) noexcept;
        ~GraphicsPipeline() noexcept;

        [[nodiscard]] constexpr auto *GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12RootSignature> m_root_signature;
    };

    class ComputePipeline final : public DnmGL::ComputePipeline  {
    public:
        ComputePipeline(D3D12::Context& context, const DnmGL::ComputePipelineDesc& desc) noexcept;
        ~ComputePipeline() noexcept;

        [[nodiscard]] constexpr auto *GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto *GetRootSignature() const noexcept { return m_root_signature.Get(); }
    private:
        ComPtr<ID3D12PipelineState> m_pipeline_state;
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
            pipeline_state = std::move(m_pipeline_state),
            root_signature = std::move(m_root_signature)
        ] () mutable {
            pipeline_state.Reset();
            root_signature.Reset();
        });
    }
}