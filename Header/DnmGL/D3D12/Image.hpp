#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    constexpr D3D12_RESOURCE_STATES GetIdealImageState(ImageUsageFlags usage_flags) {
        if (usage_flags == ImageUsageBits::eColorAttachment
        || usage_flags == (ImageUsageBits::eColorAttachment | ImageUsageBits::eTransientAttachment)) {
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        else if (usage_flags == ImageUsageBits::eDepthStencilAttachment
        || usage_flags == (ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment)) {
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        else if (usage_flags == ImageUsageBits::eReadonlyResource) {
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
        else if (usage_flags == ImageUsageBits::eWritebleResource) {
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        return D3D12_RESOURCE_STATE_COMMON;
    }

    class Image final : public DnmGL::Image {
    public:
        Image(D3D12::Context& context, const DnmGL::ImageDesc& desc);
        ~Image() noexcept;

        [[nodiscard]] auto* GetResource() const noexcept { return m_image.Get(); }
        [[nodiscard]] auto* GetAllocation() const noexcept { return m_allocation; }
        [[nodiscard]] auto GetState() const noexcept { return m_state; }
        [[nodiscard]] auto GetIdealState() const noexcept { return GetIdealImageState(m_desc.usage_flags); }
    private:
        ComPtr<ID3D12Resource2> m_image;
        D3D12MA::Allocation* m_allocation;

        D3D12_RESOURCE_STATES m_state;
        friend D3D12::CommandBuffer;
    };

    inline Image::~Image() noexcept {
        D3D12Context->AddDeferDelete([
            Image = std::move(m_image),
            allocation = m_allocation
        ] () mutable {
            Image.Reset();
            allocation->Release();
        });
    }
}