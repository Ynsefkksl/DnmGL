#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class CommandBuffer final : public DnmGL::CommandBuffer {
    public:
        CommandBuffer(D3D12::Context& context);
        ~CommandBuffer() noexcept = default;
    
        [[nodiscard]] ID3D12CommandList* GetCommandList() const noexcept { return m_command_list.Get(); }

        void IBegin() override;
        void IEnd() override;

        void IBeginRendering(const BeginRenderingDesc&) override { DnmGLAssert(false, "this func is not complate") }
        void IEndRendering() override { DnmGLAssert(false, "this func is not complate") }

        void IBeginCopyPass() override {}
        void IEndCopyPass() override {}

        void IBeginComputePass() override {}
        void IEndComputePass() override {}

        void IUploadData(DnmGL::Image *image, 
                        const ImageSubresource& subresource, 
                        const void* data, 
                        Uint3 copy_extent, 
                        Uint3 copy_offset) override;
        void IUploadData(DnmGL::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) override;
    
        void ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) override;
        void ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) override;
        void ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) override;
        void ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) override;
    
        void IDispatch(uint32_t x = 1, uint32_t y = 1, uint32_t z = 1) override;

        void IBindPipeline(const DnmGL::ComputePipeline* pipeline) override;

        void IGenerateMipmaps(DnmGL::Image* image) override { DnmGLAssert(false, "this func is not complate") }
    
        void IBindVertexBuffer(const DnmGL::Buffer* buffer, uint64_t offset) override { DnmGLAssert(false, "this func is not complate") }
        void IBindIndexBuffer(const DnmGL::Buffer* buffer, uint64_t offset, DnmGL::IndexType index_type) override { DnmGLAssert(false, "this func is not complate") }
    
        void IDraw(uint32_t vertex_count, uint32_t instance_count) override { DnmGLAssert(false, "this func is not complate") }
        void IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) override { DnmGLAssert(false, "this func is not complate") }
    
        void ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) override { DnmGLAssert(false, "this func is not complate") }
        void ISetScissor(Uint2 extent, Uint2 offset) override { DnmGLAssert(false, "this func is not complate") }
    private:
        ComPtr<ID3D12GraphicsCommandList> m_command_list;

        friend D3D12::Context;
    };
    
    inline void CommandBuffer::IBegin() {
        m_command_list->Reset(D3D12Context->GetCommandAllocator(), nullptr);
    }

    inline void CommandBuffer::IEnd() {
        m_command_list->Close();
    }

    inline void CommandBuffer::IDispatch(uint32_t x, uint32_t y, uint32_t z) {
        m_command_list->Dispatch(x, y, z);
    }
}