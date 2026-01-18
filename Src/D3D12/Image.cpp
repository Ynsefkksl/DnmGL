#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/ToDxgiFormat.hpp"

namespace DnmGL::D3D12 {
    static constexpr D3D12_RESOURCE_FLAGS GetResourceFlags(ImageUsageFlags flags) {
        D3D12_RESOURCE_FLAGS d3d12_flags{};
        if (flags.Has(ImageUsageBits::eWritebleResource)) {
            d3d12_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if (flags.Has(ImageUsageBits::eColorAttachment)) {
            d3d12_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (flags.Has(ImageUsageBits::eDepthStencilAttachment)) {
            d3d12_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (!flags.Has(ImageUsageBits::eReadonlyResource)) {
            d3d12_flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        return d3d12_flags;
    }

    static constexpr D3D12_RESOURCE_DIMENSION GetDimantion(ImageType type) {
        switch (type) {
            case ImageType::e1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            case ImageType::e2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            case ImageType::e3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        }
    }

    Image::Image(D3D12::Context& ctx, const DnmGL::ImageDesc& desc)
    : DnmGL::Image(ctx, desc) {
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = GetDimantion(desc.type);
        resourceDesc.Alignment = 0;
        resourceDesc.Width = m_desc.extent.x;
        resourceDesc.Height = m_desc.extent.y;
        resourceDesc.DepthOrArraySize = m_desc.extent.z;
        resourceDesc.MipLevels = m_desc.mipmap_levels;
        resourceDesc.Format = ToDxgiFormat(m_desc.format);
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = GetResourceFlags(m_desc.usage_flags);
        resourceDesc.SampleDesc.Count = uint32_t(desc.sample_count);
        resourceDesc.SampleDesc.Quality = 0;
        
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_STATES initial_state = GetIdealImageState(m_desc.usage_flags);

        auto hr = D3D12Context->GetAllocator()->CreateResource(
            &allocationDesc,
            &resourceDesc,
            initial_state,
            NULL,
            &m_allocation,
            IID_PPV_ARGS(&m_image));

        if (hr == E_OUTOFMEMORY) {
            context->Message(std::format("CreateResource failed, Error: out of memory"), MessageType::eOutOfMemory);
        }
        else if(FAILED(hr)) {
            context->Message(std::format("CreateResource failed, Error: {}", HresultToString(hr)), MessageType::eUnknown);
        }

        m_state = initial_state;
    }
}