#include "DnmGL/D3D12/Sampler.hpp"

namespace DnmGL::D3D12 {
    static constexpr uint32_t GetMaxAnisotropy(const SamplerFilter filter) {
        switch (filter) {
            case SamplerFilter::eNearest:
            case SamplerFilter::eLinear: return 1u;
            case SamplerFilter::eAnisotropyX2: return 2u;
            case SamplerFilter::eAnisotropyX4: return 4u;
            case SamplerFilter::eAnisotropyX8: return 8u;
            case SamplerFilter::eAnisotropyX16: return 16u;
        }
    }

    static constexpr D3D12_COMPARISON_FUNC GetCompareFunc(const CompareOp compare_op) {
        switch (compare_op) {
            case CompareOp::eNone: return D3D12_COMPARISON_FUNC_NONE;
            case CompareOp::eNever: return D3D12_COMPARISON_FUNC_NEVER;
            case CompareOp::eLess: return D3D12_COMPARISON_FUNC_LESS;
            case CompareOp::eEqual: return D3D12_COMPARISON_FUNC_EQUAL;
            case CompareOp::eLessOrEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case CompareOp::eGreater: return D3D12_COMPARISON_FUNC_GREATER;
            case CompareOp::eNotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case CompareOp::eGreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case CompareOp::eAlways: return D3D12_COMPARISON_FUNC_ALWAYS;
        }
    }

    static constexpr D3D12_TEXTURE_ADDRESS_MODE GetAdressMode(const SamplerAddressMode address_mode) {
        switch (address_mode) {
            case SamplerAddressMode::eRepeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            case SamplerAddressMode::eMirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case SamplerAddressMode::eClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case SamplerAddressMode::eClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        }
    }

    static constexpr D3D12_FILTER GetFilter(const SamplerFilter filter, const SamplerMipmapMode sampler_mipmap_mode, const CompareOp compare_op) {
        const bool mip_mode_nearest = sampler_mipmap_mode == SamplerMipmapMode::eNearest;
        if (compare_op == CompareOp::eNone) {
            switch (filter) {
                case SamplerFilter::eNearest: return mip_mode_nearest ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                case SamplerFilter::eLinear: return mip_mode_nearest ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                case SamplerFilter::eAnisotropyX2:
                case SamplerFilter::eAnisotropyX4:
                case SamplerFilter::eAnisotropyX8:
                case SamplerFilter::eAnisotropyX16: return D3D12_FILTER_ANISOTROPIC;
            }
        }
        else {
            switch (filter) {
                case SamplerFilter::eNearest: return mip_mode_nearest ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
                case SamplerFilter::eLinear: return mip_mode_nearest ? D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
                case SamplerFilter::eAnisotropyX2:
                case SamplerFilter::eAnisotropyX4:
                case SamplerFilter::eAnisotropyX8:
                case SamplerFilter::eAnisotropyX16: return D3D12_FILTER_COMPARISON_ANISOTROPIC;
            }
        }
    }

    Sampler::Sampler(DnmGL::D3D12::Context& ctx, const DnmGL::SamplerDesc& desc)
        : DnmGL::Sampler(ctx, desc), m_sampler_desc({
            .Filter = GetFilter(m_desc.filter, m_desc.mipmap_mode, m_desc.compare_op),
            .AddressU = GetAdressMode(m_desc.address_mode_u),
            .AddressV = GetAdressMode(m_desc.address_mode_v),
            .AddressW = GetAdressMode(m_desc.address_mode_w),
            .MipLODBias = 0.f,
            .MaxAnisotropy = GetMaxAnisotropy(m_desc.filter),
            .ComparisonFunc = GetCompareFunc(m_desc.compare_op),
            .BorderColor = {0,0,0,0},
            .MinLOD = 0.f,
            .MaxLOD = float(1 << 16),
        }) {}
}