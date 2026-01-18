#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class Sampler final : public DnmGL::Sampler {
    public:
        Sampler(DnmGL::D3D12::Context& context, const DnmGL::SamplerDesc& desc);
        ~Sampler() = default;

        const D3D12_SAMPLER_DESC m_sampler_desc;
    };
}