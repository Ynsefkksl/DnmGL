#pragma once

#include "DnmGL/DnmGL.hpp"
#include "DnmGL/Utility/Macros.hpp"

#ifdef OS_WIN
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define VK_USE_PLATFORM_WIN32_KHR
    
    #include <vulkan/vulkan.hpp>

    #undef UpdateResource
    #undef far
    #undef near
#else
    #include <vulkan/vulkan.hpp>
#endif

#include <functional>
#include <vector>
#include <cstdint>

#define VMA_VULKAN_VERSION 1001000
#include <vma/vk_mem_alloc.h>

#define VulkanContext reinterpret_cast<Vulkan::Context*>(context)
#define DECLARE_VK_FUNC(func_name) PFN_##func_name func_name

namespace DnmGL::Vulkan {
    class CommandBuffer;
    class Shader;
    class Buffer;
    class Image;
    class Sampler;
    class GraphicsPipelineBase;
    class ComputePipeline;
    class Sampler;
    class FramebufferBase;
    class FramebufferDynamicRendering;

    // images must be this layout except for copy or transfer commands  
    constexpr vk::ImageLayout GetIdealImageLayout(DnmGL::ImageUsageFlags flags) {
        // in BeginRendering color and depthStencil resources translated to color or depthStencil layout
        if (flags.Has(DnmGL::ImageUsageBits::eReadonlyResource)) {
            if (flags.Has(DnmGL::ImageUsageBits::eWritebleResource))
                return vk::ImageLayout::eGeneral;
            
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        }
        
        if (flags.Has(DnmGL::ImageUsageBits::eDepthStencilAttachment)) {
            return vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }

        if (flags.Has(DnmGL::ImageUsageBits::eColorAttachment)) {
            return vk::ImageLayout::eColorAttachmentOptimal;
        }

        return vk::ImageLayout::eGeneral;
    }

    struct SwapchainProperties {
        vk::Format format;
        vk::ColorSpaceKHR color_space;
        vk::PresentModeKHR present_mode;
        vk::Extent2D extent;
        uint32_t image_count;
    
        operator std::string() {
            return std::format(
                "\nSwapchain Properties \nformat: {} \ncolor space: {} \npresent mode: {} \nextent: {}, {} \nimage count: {}\n", 
                                        vk::to_string(format),
                                        vk::to_string(color_space),
                                        vk::to_string(present_mode),
                                        extent.width, extent.height,
                                        image_count);
        }
    };
    
    [[nodiscard]] constexpr vk::AttachmentLoadOp ToVk(AttachmentLoadOp v) noexcept {
        return static_cast<vk::AttachmentLoadOp>(v);
    }

    [[nodiscard]] constexpr vk::AttachmentStoreOp ToVk(AttachmentStoreOp v) noexcept {
        return static_cast<vk::AttachmentStoreOp>(v);
    }

    [[nodiscard]] constexpr vk::ShaderStageFlags ToVk(ShaderStageFlags v) noexcept {
        vk::ShaderStageFlags out{}; 
        if (v.Has(ShaderStageBits::eVertex)) {
            out |= vk::ShaderStageFlagBits::eVertex;
        }
        if (v.Has(ShaderStageBits::eFragment)) {
            out |= vk::ShaderStageFlagBits::eFragment;
        }
        if (v.Has(ShaderStageBits::eCompute)) {
            out |= vk::ShaderStageFlagBits::eCompute;
        }
        return out;
    }

    class Context final : public DnmGL::Context {
    public:
        struct InternalImageLayoutTranslation {
            vk::Image image;
            bool has_stencil;
            vk::ImageLayout layout;
        };

        struct InternalBufferResource {
            vk::Buffer buffer;
            vk::DescriptorType type;
            uint32_t offset;
            uint32_t size;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct InternalImageResource {
            vk::ImageView image_view;
            vk::ImageLayout image_layout;
            vk::DescriptorType type;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct InternalSamplerResource {
            vk::Sampler sampler;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct SupportedFeatures {
            bool uniform_buffer_update_after_bind : 1{};
            bool storage_buffer_update_after_bind : 1{};
            bool storage_image_update_after_bind : 1{};
            bool sampled_image_update_after_bind : 1{};

            bool memory_budget : 1{};
            bool pageable_device_local_memory : 1{};
            bool memory_priority : 1{};
            bool sync2 : 1{};
            bool anisotropy : 1{};
            bool dynamic_rendering : 1{};

            //chatgpt
            operator std::string() {
                std::string s("\nSupported Features: \n");
                s += "uniform_buffer_update_after_bind: " + std::string(uniform_buffer_update_after_bind ? "true" : "false") + "\n";
                s += "storage_buffer_update_after_bind: " + std::string(storage_buffer_update_after_bind ? "true" : "false") + "\n";
                s += "storage_image_update_after_bind: " + std::string(storage_image_update_after_bind ? "true" : "false") + "\n";
                s += "sampled_image_update_after_bind: " + std::string(sampled_image_update_after_bind ? "true" : "false") + "\n";
                s += "memory_budget: " + std::string(memory_budget ? "true" : "false") + "\n";
                s += "pageable_device_local_memory: " + std::string(pageable_device_local_memory ? "true" : "false") + "\n";
                s += "memory_priority: " + std::string(memory_priority ? "true" : "false") + "\n";
                s += "sync2: " + std::string(sync2 ? "true" : "false") + "\n";
                s += "anisotropy: " + std::string(anisotropy ? "true" : "false") + "\n";
                s += "dynamic_rendering: " + std::string(dynamic_rendering ? "true" : "false") + "\n";
                s += "\n";
                return s;
            }
        };
        
        struct DeviceFeatures {
            vk::SampleCountFlags supported_samples;
            vk::QueueFlags queue_flags;
            uint32_t timestamp_valid_bits;
            uint32_t queue_family;
        };
    public:
        Context() = default;
        ~Context();

        [[nodiscard]] constexpr GraphicsBackend GetGraphicsBackend() const noexcept override {
            return GraphicsBackend::eVulkan;
        }

        void IInit(const ContextDesc&) override;
        void ISetSwapchainSettings(const SwapchainSettings& settings) override;

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

        [[nodiscard]] constexpr auto GetInstance() const noexcept { return m_instance; }
        [[nodiscard]] constexpr auto GetSurface() const noexcept { return m_surface; }
        [[nodiscard]] constexpr auto GetDevice() const noexcept { return m_device; }
        [[nodiscard]] constexpr auto GetPhysicalDevice() const noexcept { return m_physical_device; }
        [[nodiscard]] constexpr auto GetQueue() const noexcept { return m_queue; }
        [[nodiscard]] constexpr auto GetSwapchain() const noexcept { return m_swapchain; }
        [[nodiscard]] constexpr auto GetCommandPool() const noexcept { return m_command_pool; }
        [[nodiscard]] constexpr auto GetPipelineCache() const noexcept { return m_pipeline_cache; }
        [[nodiscard]] constexpr auto GetDescriptorPool() const noexcept { return m_descriptor_pool; }
        [[nodiscard]] constexpr const auto& GetSwapchainImages() const noexcept { return m_swapchain_images; }
        [[nodiscard]] constexpr const auto& GetSwapchainImageViews() const noexcept { return m_swapchain_image_views; }
        [[nodiscard]] constexpr auto* GetCommandBuffer() const noexcept { return m_command_buffer; }
        [[nodiscard]] constexpr auto* GetVmaAllocator() const noexcept { return m_vma_allocator; }
        [[nodiscard]] constexpr auto* GetDepthBuffer() const noexcept { return m_depth_buffer; }
        [[nodiscard]] constexpr auto* GetResolveImage() const noexcept { return m_resolve_image; }
        [[nodiscard]] constexpr auto GetSwapchainProperties() const noexcept { return m_swapchain_properties; }
        [[nodiscard]] constexpr auto GetImageIndex() const noexcept { return m_image_index; }
        [[nodiscard]] constexpr auto GetEmptySetLayout() const noexcept { return m_empty_set_layout; }
        [[nodiscard]] constexpr auto GetEmptySet() const noexcept { return m_empty_set; }
        [[nodiscard]] constexpr const auto& GetDispatcher() const noexcept { return dispatcher; }
        [[nodiscard]] DnmGL::ContextState GetContextState() noexcept override;
        [[nodiscard]] vk::SampleCountFlagBits GetSampleCount(DnmGL::SampleCount sample_count, bool has_stencil) const noexcept;

        vk::Framebuffer GetOrCreateFramebuffer(vk::RenderPass renderpass) noexcept;
        void ReCreateFramebuffersForSwapchainChanges() noexcept;
    
        [[nodiscard]]auto GetSupportedFeatures() const { return supported_features; }
        [[nodiscard]]auto GetDeviceFeatures() const { return device_features; }

        void DeleteObject(const std::function<void(vk::Device device, VmaAllocator allocator)>& delete_func) {
            defer_vulkan_obj_delete.emplace_back(std::move(delete_func));
        }

        Vulkan::CommandBuffer* GetCommandBufferIfRecording();
        //just for new created images

        void DeferResourceUpdate(const std::span<const InternalBufferResource>& res);
        void DeferResourceUpdate(const std::span<const InternalImageResource>& res);
        void DeferResourceUpdate(const std::span<const InternalSamplerResource>& res);

        void ProcessResource(
            const Context::InternalBufferResource& res, vk::DescriptorBufferInfo& info, vk::WriteDescriptorSet& write);
        void ProcessResource(
            const Context::InternalImageResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write);
        void ProcessResource(
            const Context::InternalSamplerResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write);
    private:
        std::vector<std::function<void(vk::Device device, VmaAllocator allocator)>> defer_vulkan_obj_delete;

        using InternalResource = std::variant<InternalBufferResource, InternalImageResource, InternalSamplerResource>;
        //just for new created images
        std::vector<InternalResource> defer_resource_update;
        void ProcessResourceUpdates();
        void DeleteVulkanObjects();
        void CreateFramebuffers(vk::RenderPass renderpass, std::vector<vk::Framebuffer>& out_framebuffers) noexcept;

        struct Dispatcher {
            constexpr uint32_t getVkHeaderVersion() const { return VK_HEADER_VERSION; }
            DECLARE_VK_FUNC(vkCreateDebugUtilsMessengerEXT);
            DECLARE_VK_FUNC(vkDestroyDebugUtilsMessengerEXT);
            DECLARE_VK_FUNC(vkCmdPipelineBarrier2KHR);
            DECLARE_VK_FUNC(vkCmdBeginRenderingKHR);
            DECLARE_VK_FUNC(vkCmdEndRenderingKHR);
        } dispatcher;

        SupportedFeatures supported_features;
        DeviceFeatures device_features;

        void CreateInstance(WindowType window_type);
        void CreateDebugMessenger();
        void CreateSurface(const WindowHandle&);
        void CreateDevice();
        void CreateCommandPool();
        void CreateDescriptorPool();
        void CreateSwapchain(Uint2 extent, bool Vsync);
        void CreateVmaAllocator();
        void CreatePipelineCache();
        void CreateResource();
        void CreateDepthBuffer(Uint2 extent, SampleCount sample_count, ImageFormat format);
        void CreateResolveImage(Uint2 extent, SampleCount sample_count);
        
        vk::Instance m_instance = VK_NULL_HANDLE;
        vk::DebugUtilsMessengerEXT m_debug_messanger = VK_NULL_HANDLE;
        vk::SurfaceKHR m_surface = VK_NULL_HANDLE;
        vk::PhysicalDevice m_physical_device = VK_NULL_HANDLE;
        vk::Device m_device = VK_NULL_HANDLE;
        vk::Queue m_queue = VK_NULL_HANDLE;
        vk::PipelineCache m_pipeline_cache = VK_NULL_HANDLE;
        vk::Fence m_fence = VK_NULL_HANDLE;
        vk::Semaphore m_acquire_next_image_semaphore = VK_NULL_HANDLE;
        vk::Semaphore m_render_finished_semaphore = VK_NULL_HANDLE;
        vk::CommandPool m_command_pool = VK_NULL_HANDLE;
        CommandBuffer* m_command_buffer;
        vk::SwapchainKHR m_swapchain = VK_NULL_HANDLE;
        std::vector<vk::Image> m_swapchain_images{};
        std::vector<vk::ImageView> m_swapchain_image_views{};
        std::unordered_map<VkRenderPass, std::vector<vk::Framebuffer>> m_framebuffers;
        VmaAllocator m_vma_allocator = VK_NULL_HANDLE;
        vk::DescriptorPool m_descriptor_pool;
        vk::DescriptorSetLayout m_empty_set_layout;
        vk::DescriptorSet m_empty_set;
        SwapchainProperties m_swapchain_properties;
        SwapchainSettings m_swapchain_settings;
        Image* m_depth_buffer;
        //for msaa
        Image* m_resolve_image;
        uint32_t m_image_index{};
        ContextState context_state = ContextState::eNone;
    };

    inline void Context::WaitForGPU() {
        if (m_device.getFenceStatus(m_fence) == vk::Result::eSuccess) return;
        [[maybe_unused]] auto _ = m_device.waitForFences(m_fence, vk::True, 1'000'000'000);
    }

    inline ContextState Context::GetContextState() noexcept {
        if (context_state == ContextState::eCommandExecuting) {
            if (m_device.getFenceStatus(m_fence) == vk::Result::eSuccess)
                context_state = ContextState::eNone;
        }
        return context_state;
    }

    inline Vulkan::CommandBuffer* Context::GetCommandBufferIfRecording() {
        if (context_state == ContextState::eCommandBufferRecording) {
            return m_command_buffer;
        }
        return nullptr;
    }

    inline void Context::ProcessResource(const Context::InternalBufferResource& res, vk::DescriptorBufferInfo& info, vk::WriteDescriptorSet& write) {
        info.setBuffer(res.buffer)
                .setOffset(res.offset)
                .setRange(res.size);

        write.setBufferInfo({info})
                .setDescriptorCount(1)
                .setDstArrayElement(res.array_element)
                .setDescriptorType(res.type)
                .setDstBinding(res.binding)
                .setDstSet(res.set);
    }

    inline void Context::ProcessResource(const Context::InternalImageResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write) {
        info.setImageView(res.image_view)
            .setImageLayout(res.image_layout)
            ;

        write.setImageInfo({info})
                .setDescriptorCount(1)
                .setDstArrayElement(res.array_element)
                .setDescriptorType(res.type)
                .setDstBinding(res.binding)
                .setDstSet(res.set);
    }

    inline void Context::ProcessResource(const Context::InternalSamplerResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write) {
        info.setSampler(res.sampler);

        write.setImageInfo({info})
            .setDescriptorCount(1)
            .setDstArrayElement(res.array_element)
            .setDescriptorType(vk::DescriptorType::eSampler)
            .setDstBinding(res.binding)
            .setDstSet(res.set);
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalBufferResource>& res) {
        defer_resource_update.insert(defer_resource_update.end(), res.begin(), res.end());
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalImageResource>& res) {
        defer_resource_update.insert(defer_resource_update.end(), res.begin(), res.end());
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalSamplerResource>& res) {
        defer_resource_update.insert(defer_resource_update.end(), res.begin(), res.end());
    }

    inline void Context::DeleteVulkanObjects() {
        for (const auto& delete_func : defer_vulkan_obj_delete) {
            delete_func(m_device, m_vma_allocator);
        }
        
        defer_vulkan_obj_delete.clear();
    }

    inline vk::SampleCountFlagBits Context::GetSampleCount(DnmGL::SampleCount sample_count, bool has_stencil) const noexcept {
        const auto properties = m_physical_device.getProperties();
        //framebuffer and sampled support same
        //color and stencil support same
        vk::SampleCountFlags supported_samples = properties.limits.framebufferColorSampleCounts;
        if (has_stencil) supported_samples |= properties.limits.framebufferStencilSampleCounts;
        //TODO: check integer format and storage buffer msaa support

        switch (sample_count) {
            case SampleCount::e1: return vk::SampleCountFlagBits::e1;
            case SampleCount::e2:
                if (vk::SampleCountFlagBits::e2 & supported_samples) {
                    return vk::SampleCountFlagBits::e2;
                }
                else if (vk::SampleCountFlagBits::e4 & supported_samples) {
                    return vk::SampleCountFlagBits::e4;
                }
            case SampleCount::e4:
                if (vk::SampleCountFlagBits::e4 & supported_samples) {
                    return vk::SampleCountFlagBits::e4;
                }
                else if (vk::SampleCountFlagBits::e8 & supported_samples) {
                    return vk::SampleCountFlagBits::e8;
                }
                else if (vk::SampleCountFlagBits::e2 & supported_samples) {
                    return vk::SampleCountFlagBits::e2;
                }
            case SampleCount::e8:
                if (vk::SampleCountFlagBits::e8 & supported_samples) {
                    return vk::SampleCountFlagBits::e8;
                }
                else if (vk::SampleCountFlagBits::e4 & supported_samples) {
                    return vk::SampleCountFlagBits::e4;
                }
                else if (vk::SampleCountFlagBits::e2 & supported_samples) {
                    return vk::SampleCountFlagBits::e2;
                }
        }
        return vk::SampleCountFlagBits::e1;
    }
}