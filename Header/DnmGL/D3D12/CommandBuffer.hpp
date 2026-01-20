#pragma once

#include "DnmGL/D3D12/Context.hpp"

namespace DnmGL::D3D12 {
    class CommandBuffer final : public DnmGL::CommandBuffer {
    public:
        CommandBuffer(D3D12::Context& context);
        ~CommandBuffer() noexcept = default;
    
        [[nodiscard]] ID3D12CommandList *GetCommandList() const noexcept { return m_command_list.Get(); }

        void IBegin() override;
        void IEnd() override;

        void IBeginRendering(const BeginRenderingDesc&) override;
        void IEndRendering() override;

        void IBeginCopyPass() override {}
        void IEndCopyPass() override {}

        void IBeginComputePass() override {}
        void IEndComputePass() override {}

        void IUploadData(DnmGL::Image *image, 
                        const ImageSubresource& subresource, 
                        const void *data, 
                        Uint3 copy_extent, 
                        Uint3 copy_offset) override;
        void IUploadData(DnmGL::Buffer *buffer, const void *data, uint32_t size, uint32_t offset) override;
    
        void ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) override;
        void ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) override;
        void ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) override;
        void ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) override;
    
        void IDispatch(uint32_t x = 1, uint32_t y = 1, uint32_t z = 1) override;

        void IBindPipeline(const DnmGL::ComputePipeline *pipeline) override;

        void IGenerateMipmaps(DnmGL::Image *image) override { DnmGLAssert(false, "this func is not complate") }
    
        void IBindVertexBuffer(const DnmGL::Buffer *buffer, uint64_t offset) override;
        void IBindIndexBuffer(const DnmGL::Buffer *buffer, uint64_t offset, DnmGL::IndexType index_type) override;
    
        void IDraw(uint32_t vertex_count, uint32_t instance_count) override;
        void IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) override;
    
        void ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) override;
        void ISetScissor(Uint2 extent, Uint2 offset) override;
    private:
        void FillBeginRenderpassForPresenting(
            std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> &render_target_descs, 
            D3D12_RENDER_PASS_DEPTH_STENCIL_DESC &depth_stencil_desc,
            const BeginRenderingDesc &desc);

        void FillBeginRenderpassForFramebuffer(
            std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> &render_target_descs, 
            D3D12_RENDER_PASS_DEPTH_STENCIL_DESC &depth_stencil_desc,
            const BeginRenderingDesc &desc,
            D3D12::Framebuffer &framebuffer);


        ComPtr<ID3D12GraphicsCommandList10> m_command_list;

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

    inline void CommandBuffer::IDraw(uint32_t vertex_count, uint32_t instance_count) {
        m_command_list->DrawInstanced(vertex_count, instance_count, 0, 0);
    }

    inline void CommandBuffer::IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) {
        m_command_list->DrawIndexedInstanced(index_count, instance_count, 0, 0, 0);
    }

    inline void CommandBuffer::ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) {
        const D3D12_VIEWPORT viewport {
            0,
            0,
            extent.x,
            extent.y,
            min_depth,
            max_depth
        };
        m_command_list->RSSetViewports(1, &viewport);
    }

    inline void CommandBuffer::ISetScissor(Uint2 extent, Uint2 offset) {
        const D3D12_RECT scissor {
            static_cast<LONG>(offset.x),
            static_cast<LONG>(offset.y),
            static_cast<LONG>(extent.x),
            static_cast<LONG>(extent.y),
        };
        m_command_list->RSSetScissorRects(1, &scissor);
    }
}