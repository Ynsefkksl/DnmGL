#include "DnmGL/D3D12/Context.hpp"
#include "DnmGL/D3D12/CommandBuffer.hpp"
#include "DnmGL/D3D12/Buffer.hpp"
#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/Shader.hpp"
#include "DnmGL/D3D12/Pipeline.hpp"
#include "DnmGL/D3D12/Sampler.hpp"
#include "DnmGL/D3D12/ResourceManager.hpp"
#include "DnmGL/D3D12/Framebuffer.hpp"

extern "C" __declspec(dllexport) DnmGL::Context* LoadContext() {
    return new DnmGL::D3D12::Context();
}

namespace DnmGL::D3D12 {
    static void GetHardwareAdapter(IDXGIFactory6* factory, IDXGIAdapter1** out_adapter) {
        *out_adapter = nullptr;
        ComPtr<IDXGIAdapter1> adapter;

        uint32_t adapter_index = 0;
        while (true) {
            SUCCEEDED(factory->EnumAdapterByGpuPreference(
                adapter_index++,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter)));

            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(), 
                D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }

        *out_adapter = adapter.Detach();
    }

    Context::~Context() {
        WaitForGPU();

        CloseHandle(m_fence_event);

        delete placeholder_image;
        delete placeholder_sampler;
        delete m_command_buffer;

        DeleteObjects();
        m_allocator->Release();
    }

    void Context::IInit(const ContextDesc& desc) {
        shader_directory = desc.shader_directory;
        if (GetWindowType(desc.window_handle) != DnmGL::WindowType::eWindows) {
            Message("window handle must be windows in d3d12 context, how did you do that?", MessageType::eInvalidBehavior);
        }
        WinWindowHandle window_handle = std::get<WinWindowHandle>(desc.window_handle);

        if constexpr (_debug) {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
                debug_controller->EnableDebugLayer();
            }
    
            ComPtr<ID3D12Debug1> debug1;
            if (SUCCEEDED(debug_controller.As(&debug1))) {
                debug1->SetEnableGPUBasedValidation(TRUE);
            }
        }

        ComPtr<IDXGIFactory6> factory;
        if (auto hr = CreateDXGIFactory2(_debug * DXGI_CREATE_FACTORY_DEBUG,
                            __uuidof(IDXGIFactory6),
                            IID_PPV_ARGS_Helper(&factory));
            FAILED(hr)) {
            Message(std::format("CreateDXGIFactory2 failed, Error: {}", HresultToString(hr)), MessageType::eUnsupportedDevice);
            return;
        }

        ComPtr<IDXGIAdapter1> hardware_adapter;
        GetHardwareAdapter(factory.Get(), &hardware_adapter);

        D3D12CreateDevice(
            hardware_adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device)
            );

        if constexpr (_debug) {
            ComPtr<ID3D12InfoQueue> iq;
            if (SUCCEEDED(m_device.As(&iq))) {}
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_command_queue));

        DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
        swapchain_desc.BufferCount = FrameCount;
        swapchain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.BufferDesc.Width = desc.swapchain_settings.window_extent.x;
        swapchain_desc.BufferDesc.Height = desc.swapchain_settings.window_extent.y;
        swapchain_desc.OutputWindow = HWND(window_handle.hwnd);
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.Windowed = TRUE;
        swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ;

        ComPtr<IDXGISwapChain> swapchain;
        factory->CreateSwapChain(
            m_command_queue.Get(),
            &swapchain_desc,
            &swapchain
            );

        swapchain.As(&m_swapchain);

        factory->MakeWindowAssociation(HWND(window_handle.hwnd), DXGI_MWA_NO_ALT_ENTER);

        m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

        {
            D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
            rtv_heap_desc.NumDescriptors = FrameCount;
            rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            m_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_rtv_heap));

            m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            m_sampler_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            m_CBV_SRV_UAV_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart());

            for (auto i : Counter(FrameCount)) {
                m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i]));
                m_device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, rtv_handle);
                rtv_handle.Offset(1, m_rtv_descriptor_size);
            }
        }

        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator));

        {
            m_device->CreateFence(0, D3D12_FENCE_FLAGS::D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
            m_fence_value = 1;

            m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        }
        
        m_command_buffer = new CommandBuffer(*this);
        m_command_buffer->End();

        {
            D3D12MA::ALLOCATOR_DESC allocator_desc{};
            allocator_desc.pDevice = m_device.Get();
            allocator_desc.pAdapter = hardware_adapter.Get();
            allocator_desc.Flags = 
                D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED
                | D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED
                | D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED
                ;

            D3D12MA::CreateAllocator(
                &allocator_desc, 
                &m_allocator);
        }
        
        D3D12_FEATURE_DATA_D3D12_OPTIONS16 opts16 = {};

        m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &opts16,
        sizeof(opts16));

        m_device_features.gpu_upload_heap = opts16.GPUUploadHeapSupported;

        CreateResources();
    }

    void Context::ExecuteCommands(const std::function<bool(DnmGL::CommandBuffer*)>& func) {
        WaitForGPU();

        DeleteObjects();
        
        m_command_allocator->Reset();
        context_state = ContextState::eCommandBufferRecording;
        m_command_buffer->Begin();
        {
            if (!func(m_command_buffer)) {
                m_command_buffer->End();
                return;
            }
        }
        m_command_buffer->End();

        ID3D12CommandList *const command_lists[1] = { m_command_buffer->GetCommandList() };
        m_command_queue->ExecuteCommandLists(1, command_lists);

        m_command_queue->Signal(m_fence.Get(), m_fence_value++);

        context_state = ContextState::eCommandExecuting;
    }

    void Context::Render(const std::function<bool(DnmGL::CommandBuffer*)>& func) {
        WaitForGPU();
        
        DeleteObjects();

        m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

        m_command_allocator->Reset();
        context_state = ContextState::eCommandBufferRecording;
        m_command_buffer->Begin();
        {
            if (!func(m_command_buffer)) {
                m_command_buffer->End();
                return;
            }
        }
        m_command_buffer->End();

        ID3D12CommandList *const command_lists[1] = { m_command_buffer->GetCommandList() };
        m_command_queue->ExecuteCommandLists(1, command_lists);

        m_swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        m_command_queue->Signal(m_fence.Get(), m_fence_value++);

        context_state = ContextState::eCommandExecuting;
    }

    void Context::CreateResources() {
        ExecuteCommands([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
            command_buffer->BeginCopyPass();
            placeholder_image = new DnmGL::D3D12::Image(*this, {
                .extent = {1, 1, 1},
                .format = DnmGL::ImageFormat::eRGBA8Norm,
                .usage_flags = DnmGL::ImageUsageBits::eReadonlyResource,
                .type = DnmGL::ImageType::e2D,
                .mipmap_levels = 1,
            });

            // m_depth_buffer = new DnmGL::Vulkan::Image(*this, {
            //     .extent = {m_swapchain_properties.extent.width, m_swapchain_properties.extent.height, 1},
            //     .format = DnmGL::ImageFormat::eD16Norm,
            //     .usage_flags = DnmGL::ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment,
            //     .type = DnmGL::ImageType::e2D,
            //     .mipmap_levels = 1
            // });

            const uint32_t pixel_data = -1;
            command_buffer->UploadData(placeholder_image, {}, std::span(&pixel_data, 1), {1,1,1}, 0);
            return true;
        });

        placeholder_sampler = new DnmGL::D3D12::Sampler(*this, {
            .compare_op = DnmGL::CompareOp::eNever,
            .filter = DnmGL::SamplerFilter::eNearest
        });
    }
    
    std::unique_ptr<DnmGL::Buffer> Context::CreateBuffer(const DnmGL::BufferDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::Buffer>(*this, desc);
    }

    std::unique_ptr<DnmGL::Image> Context::CreateImage(const DnmGL::ImageDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::Image>(*this, desc);
    }

    std::unique_ptr<DnmGL::Sampler> Context::CreateSampler(const DnmGL::SamplerDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::Sampler>(*this, desc);
    }

    std::unique_ptr<DnmGL::Shader> Context::CreateShader(std::string_view desc) noexcept {
        return std::make_unique<DnmGL::D3D12::Shader>(*this, desc);
    }

    std::unique_ptr<DnmGL::ResourceManager> Context::CreateResourceManager(std::span<const DnmGL::Shader*> desc) noexcept {
        return std::make_unique<DnmGL::D3D12::ResourceManager>(*this, desc);
    }

    std::unique_ptr<DnmGL::ComputePipeline> Context::CreateComputePipeline(const DnmGL::ComputePipelineDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::ComputePipeline>(*this, desc);
    }

    std::unique_ptr<DnmGL::GraphicsPipeline> Context::CreateGraphicsPipeline(const DnmGL::GraphicsPipelineDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::GraphicsPipeline>(*this, desc);
    }

    std::unique_ptr<DnmGL::Framebuffer> Context::CreateFramebuffer(const DnmGL::FramebufferDesc& desc) noexcept {
        return std::make_unique<DnmGL::D3D12::Framebuffer>(*this, desc);
    }
}