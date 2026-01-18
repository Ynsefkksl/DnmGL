#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    constexpr D3D12_RESOURCE_STATES GetIdealBufferState(BufferUsageFlags usage) {
        if (usage.None())
            return D3D12_RESOURCE_STATE_COMMON;
        else if (usage == BufferUsageBits::eWritebleResource)
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        else {
            D3D12_RESOURCE_STATES state;
            if (usage.Has(BufferUsageBits::eReadonlyResource))
                state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            if (usage.Has(BufferUsageBits::eUniform) |
                usage.Has(BufferUsageBits::eVertex))
                state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (usage.Has(BufferUsageBits::eIndex))
                state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (usage.Has(BufferUsageBits::eIndirect))
                state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            return state;
        }
    }

    class Buffer final : public DnmGL::Buffer {
    public:
        Buffer(D3D12::Context& context, const DnmGL::BufferDesc& desc);
        ~Buffer() noexcept;

        [[nodiscard]] auto* GetResource() const noexcept { return m_buffer.Get(); }
        [[nodiscard]] auto* GetAllocation() const noexcept { return m_allocation; }
        [[nodiscard]] auto GetState() const noexcept { return m_state; }
        [[nodiscard]] auto GetIdealState() const noexcept { return GetIdealBufferState(m_desc.usage_flags); }
    private:
        ComPtr<ID3D12Resource2> m_buffer;
        D3D12MA::Allocation* m_allocation;
        
        D3D12_RESOURCE_STATES m_state;
        friend D3D12::CommandBuffer;
    };

    inline Buffer::~Buffer() noexcept {
        m_buffer->Unmap(0, nullptr);
        D3D12Context->AddDeferDelete([
            buffer = std::move(m_buffer),
            allocation = m_allocation 
        ] () mutable {
            buffer.Reset();
            allocation->Release();
        });
    }
}