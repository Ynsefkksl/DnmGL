#pragma once

#include "DnmGL/DnmGL.hpp"
#include "DnmGL/Utility/Macros.hpp"

#ifndef OS_WIN
    #error d3d12 backend just for windows
#endif

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include <wrl.h>
#include <comdef.h>
#include <directx/d3dx12.h>
#include <d3d12sdklayers.h>
#include "D3D12MA/D3D12MemAlloc.h"

using namespace Microsoft::WRL;

#define D3D12Context reinterpret_cast<D3D12::Context *>(context)

namespace DnmGL::D3D12 {
    class CommandBuffer;
    class Shader;
    class Buffer;
    class Image;
    class Sampler;
    class GraphicsPipeline;
    class ComputePipeline;
    class Sampler;

    inline std::string HresultToString(HRESULT hr) {
        _com_error err(hr);
        return err.ErrorMessage();
    }

    class Context final : public DnmGL::Context {
    public:
        static constexpr uint32_t FrameCount = 3;
        using DeferDeleteFunc = std::function<void()>;

        Context() = default;
        ~Context();

        [[nodiscard]] constexpr GraphicsBackend GetGraphicsBackend() const noexcept override {
            return GraphicsBackend::eD3D12;
        }

        void IInit(const ContextDesc&) override;
        void ISetSwapchainSettings(const SwapchainSettings& settings) override {}

        void ExecuteCommands(const std::function<bool(DnmGL::CommandBuffer*)>& func) override;
        void Render(const std::function<bool(DnmGL::CommandBuffer*)>& func) override;
        void WaitForGPU() override;
        
        [[nodiscard]] std::unique_ptr<DnmGL::Buffer> CreateBuffer(const DnmGL::BufferDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::Image> CreateImage(const DnmGL::ImageDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::Sampler> CreateSampler(const DnmGL::SamplerDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::Shader> CreateShader(std::string_view) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::ResourceManager> CreateResourceManager(std::span<const DnmGL::Shader*>) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::ComputePipeline> CreateComputePipeline(const DnmGL::ComputePipelineDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::GraphicsPipeline> CreateGraphicsPipeline(const DnmGL::GraphicsPipelineDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGL::Framebuffer> CreateFramebuffer(const DnmGL::FramebufferDesc&) noexcept override;
        [[nodiscard]] DnmGL::ContextState GetContextState() noexcept override;

        [[nodiscard]] constexpr auto* GetSwapChain() const noexcept { return m_swapchain.Get(); }
        [[nodiscard]] constexpr auto* GetDevice() const noexcept { return m_device.Get(); }
        [[nodiscard]] constexpr auto* GetRenderTarget(uint32_t i) const noexcept { return m_render_targets[i].Get(); }
        [[nodiscard]] constexpr auto* GetCommandAllocator() const noexcept { return m_command_allocator.Get(); }
        [[nodiscard]] constexpr auto* GetCommandQueue() const noexcept { return m_command_queue.Get(); }
        [[nodiscard]] constexpr auto* GetRootSignature() const noexcept { return m_root_signature.Get(); }
        [[nodiscard]] constexpr auto* GetRTVHeap() const noexcept { return m_rtv_heap.Get(); }
        [[nodiscard]] constexpr auto* GetPipelineState() const noexcept { return m_pipeline_state.Get(); }
        [[nodiscard]] constexpr auto* GetFence() const noexcept { return m_fence.Get(); }
        [[nodiscard]] constexpr auto* GetAllocator() const noexcept { return m_allocator; }

        [[nodiscard]] constexpr auto GetRTVDescriptorSize() const noexcept { return m_rtv_descriptor_size; }
        [[nodiscard]] constexpr auto GetSamplerDescriptorSize() const noexcept { return m_rtv_descriptor_size; }
        [[nodiscard]] constexpr auto GetCBV_SRV_UAVDescriptorSize() const noexcept { return m_rtv_descriptor_size; }
        [[nodiscard]] constexpr auto GetFrameIndex() const noexcept { return m_frame_index; }

        [[nodiscard]] constexpr auto GetDeviceFeatures() const noexcept { return m_device_features; }

        D3D12::CommandBuffer* GetCommandBufferIfRecording();
        void SubmitCommands();

        void AddDeferDelete(DeferDeleteFunc&& defer_delete) {
            m_defer_delete_funcs.emplace_back(std::move(defer_delete));
        }
    private:
        std::vector<DeferDeleteFunc> m_defer_delete_funcs;
        void DeleteObjects();

        void CreateResources();

        struct DeviceFeatures {
            bool gpu_upload_heap: 1;
        } m_device_features;

        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_scissor_rect;
        ComPtr<IDXGISwapChain4> m_swapchain;
        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12Resource> m_render_targets[FrameCount];
        ComPtr<ID3D12CommandAllocator> m_command_allocator;
        ComPtr<ID3D12CommandQueue> m_command_queue;
        ComPtr<ID3D12RootSignature> m_root_signature;
        ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12Fence> m_fence;
        D3D12MA::Allocator *m_allocator;
        HANDLE m_fence_event;
        D3D12::CommandBuffer* m_command_buffer;
        uint32_t m_rtv_descriptor_size{};
        uint32_t m_sampler_descriptor_size{};
        uint32_t m_CBV_SRV_UAV_descriptor_size{};
        uint32_t m_frame_index{};
        uint64_t m_fence_value{};
        ContextState context_state = ContextState::eNone;
        bool m_vsync;
    };

    inline DnmGL::ContextState Context::GetContextState() noexcept {
        if (context_state == ContextState::eCommandBufferRecording) {
            return context_state;
        }
        else if (m_fence_value - 1 == m_fence->GetCompletedValue()) {
            return ContextState::eNone;
        }
        return ContextState::eCommandExecuting;
    }

    inline void Context::WaitForGPU() {
        const auto completed_value = m_fence->GetCompletedValue();
        if (completed_value < m_fence_value - 1) {
            m_fence->SetEventOnCompletion(m_fence_value - 1, m_fence_event);
            WaitForSingleObject(m_fence_event, INFINITE);
        }
    }

    inline D3D12::CommandBuffer* Context::GetCommandBufferIfRecording() {
        return (context_state == ContextState::eCommandBufferRecording) ? m_command_buffer : nullptr;
    }

    inline void Context::DeleteObjects() {
        for (const auto& func : m_defer_delete_funcs)
            func();

        m_defer_delete_funcs.clear();
    }
}