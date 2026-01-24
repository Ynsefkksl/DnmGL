#include "DnmGL/D3D12/Buffer.hpp"

namespace DnmGL::D3D12 {
    static constexpr D3D12_RESOURCE_FLAGS GetResourceFlags(BufferUsageFlags usage_flags) {
        D3D12_RESOURCE_FLAGS flags{};
        if (!usage_flags.Has(BufferUsageBits::eReadonlyResource)) {
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        if (usage_flags.Has(BufferUsageBits::eWritebleResource)) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        return flags;
    }

    Buffer::Buffer(D3D12::Context& ctx, const DnmGL::BufferDesc& desc)
    : DnmGL::Buffer(ctx, desc) {
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = m_desc.element_count * m_desc.element_size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = GetResourceFlags(m_desc.usage_flags);
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        if (m_desc.memory_type == MemoryType::eHostMemory) {
            if (m_desc.memory_host_access == MemoryHostAccess::eReadWrite) {
                allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
            }
            else if (m_desc.memory_host_access == MemoryHostAccess::eWrite) {
                allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            }
        }
        else if (m_desc.memory_type == MemoryType::eDeviceMemory) {
            if (m_desc.memory_host_access == MemoryHostAccess::eReadWrite
            || m_desc.memory_host_access == MemoryHostAccess::eWrite) {
                if (D3D12Context->GetDeviceFeatures().gpu_upload_heap) {
                    allocationDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
                }
                else {
                    allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
                }
            }
        }
        else {
            //TOOD: make better auto option
            if (m_desc.memory_host_access == MemoryHostAccess::eReadWrite) {
                allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
            }
            else if (m_desc.memory_host_access == MemoryHostAccess::eWrite) {
                if (D3D12Context->GetDeviceFeatures().gpu_upload_heap) {
                    allocationDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
                }
                else {
                    allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
                }
            }
            else {
                allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            }
        }

        auto hr = D3D12Context->GetAllocator()->CreateResource(
            &allocationDesc,
            &resourceDesc,
            D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
            NULL,
            &m_allocation,
            IID_PPV_ARGS(&m_buffer));

        if (hr == E_OUTOFMEMORY) {
            context->Message(std::format("CreateResource failed, Error: out of memory"), MessageType::eOutOfMemory);
        }
        else if(FAILED(hr)) {
            context->Message(std::format("CreateResource failed, Error: {}", HresultToString(hr)), MessageType::eUnknown);
        }
    
        if (m_desc.memory_host_access != MemoryHostAccess::eNone) {
            m_buffer->Map(0, nullptr, reinterpret_cast<void **>(&m_mapped_ptr));
        }
        else {
            m_mapped_ptr = nullptr;
        }
        m_state = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
    }
}