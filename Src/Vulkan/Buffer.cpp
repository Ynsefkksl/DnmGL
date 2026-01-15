#include "DnmGL/Vulkan/Buffer.hpp"

namespace DnmGL::Vulkan {
    static constexpr vk::BufferUsageFlags GetVkUsageFlags(DnmGL::BufferUsageFlags flags) {
        vk::BufferUsageFlags vk_flag{};

        if (flags.Has(BufferUsageBits::eIndex))
            vk_flag |= vk::BufferUsageFlagBits::eIndexBuffer;

        if (flags.Has(BufferUsageBits::eVertex))
            vk_flag |= vk::BufferUsageFlagBits::eVertexBuffer;

        if (flags.Has(BufferUsageBits::eIndirect))
            vk_flag |= vk::BufferUsageFlagBits::eIndirectBuffer;
            
        if (flags.Has(BufferUsageBits::eUniform))
            vk_flag |= vk::BufferUsageFlagBits::eUniformBuffer;

        if (flags.Has(BufferUsageBits::eReadonlyResource) | flags.Has(BufferUsageBits::eWritebleResource))
            vk_flag |= vk::BufferUsageFlagBits::eStorageBuffer;

        vk_flag |= vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;

        return vk_flag;
    }

    Buffer::Buffer(Vulkan::Context& ctx, const DnmGL::BufferDesc& desc)
    : DnmGL::Buffer(ctx, desc) {
        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.usage = static_cast<uint32_t>(GetVkUsageFlags(m_desc.usage_flags));
        buffer_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        buffer_create_info.size = m_desc.size;

        VmaAllocationInfo alloc_info;
        VmaAllocationCreateInfo alloc_create_info{};

        switch (desc.memory_host_access) {
            case MemoryHostAccess::eNone: break;
            case MemoryHostAccess::eReadWrite: 
                alloc_create_info.flags |= (VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT); break;
            case MemoryHostAccess::eWrite: 
                alloc_create_info.flags |= (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT); break;
        }

        switch (desc.memory_type) {
            case MemoryType::eAuto: alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO; break;
            case MemoryType::eHostMemory: alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST; break;
            case MemoryType::eDeviceMemory: alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; break;
        }

        const auto result = (vk::Result)vmaCreateBuffer(VulkanContext->GetVmaAllocator(), 
                &buffer_create_info, 
                &alloc_create_info,
                reinterpret_cast<VkBuffer*>(&m_buffer), 
                &m_allocation, 
                &alloc_info);

        if (result == vk::Result::eErrorOutOfDeviceMemory || result == vk::Result::eErrorOutOfHostMemory) {
            VulkanContext->Message("vmaCreateBuffer create buffer failed, out of memory", MessageType::eOutOfMemory);
        }
        else if (result != vk::Result::eSuccess) {
            VulkanContext->Message("vmaCreateBuffer create buffer failed, unknown", MessageType::eUnknown);
        }
        
        m_mapped_ptr = reinterpret_cast<uint8_t*>(alloc_info.pMappedData);
    }

    Buffer::~Buffer() {
        const auto buffer = m_buffer;
        auto* allocation = m_allocation;
        VulkanContext->DeleteObject(
            [buffer, allocation] ([[maybe_unused]] vk::Device device, VmaAllocator allocator) -> void {
                vmaDestroyBuffer(allocator, buffer, allocation);
            });
    }
}
