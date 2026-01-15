#include "DnmGL/Vulkan/Context.hpp"

#include "DnmGL/DnmGL.hpp"
#include "DnmGL/Vulkan/CommandBuffer.hpp"
#include "DnmGL/Vulkan/Image.hpp"
#include "DnmGL/Vulkan/Buffer.hpp"
#include "DnmGL/Vulkan/Shader.hpp"
#include "DnmGL/Vulkan/ResourceManager.hpp"
#include "DnmGL/Vulkan/Pipeline.hpp"
#include "DnmGL/Vulkan/Framebuffer.hpp"
#include "DnmGL/Vulkan/Sampler.hpp"
#include "DnmGL/Vulkan/ToVkFormat.hpp"
#include <format>
#include <print>

#include <vulkan/vulkan_profiles.hpp>

#include <algorithm>
#include <cstdint>
#include <print>
#include <string>
#include <vector>

#define DISPATCH_VK_FUNC(func_name) dispatcher.func_name = reinterpret_cast<PFN_##func_name>(m_instance.getProcAddr(#func_name))

extern "C" __declspec(dllexport) DnmGL::Context* LoadContext() {
    return new DnmGL::Vulkan::Context();
}

namespace DnmGL::Vulkan {
    static const std::vector<const char*> required_extensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        const VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        [[maybe_unused]] void* pUserData) {

        if (messageType & VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            std::println("{}", pCallbackData->pMessage);

        else if (messageSeverity & VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            std::println("{}", pCallbackData->pMessage);

        else if (messageSeverity & VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            std::println("{}", pCallbackData->pMessage);

        else if (messageSeverity & VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            std::println("{}", pCallbackData->pMessage);

        return VK_FALSE;
    }

    static std::vector<std::string> CheckInstanceLayerSupport(const std::vector<std::string>& layers) {
        std::vector<std::string> unsupported_layers;
        const auto available_layers = vk::enumerateInstanceLayerProperties();
        for (auto& layer : layers) {
            bool is_found = false;
            for (auto& available_layer : available_layers) {
                if (std::string(layer) == available_layer.layerName)
                    is_found = true;
            }
            if (is_found == false) {
                unsupported_layers.emplace_back(layer);
            }
        }

        return unsupported_layers;
    }

    static std::vector<std::string> CheckInstanceExtensionSupport(const std::vector<const char*>& extensions) {
        std::vector<std::string> unsupported_extensions;
        const auto available_extensions = vk::enumerateInstanceExtensionProperties();
        for (auto& extension : extensions) {
            bool is_found = false;
            for (auto& available_extension : available_extensions) {
                if (std::string(extension) == available_extension.extensionName)
                    is_found = true;
            }
            if (is_found == false) {
                unsupported_extensions.emplace_back(extension);
            }
        }

        return unsupported_extensions;
    }

    static bool CheckDeviceExtensionSupport(const vk::PhysicalDevice physical_device, const char* extension) {
        const auto available_extensions = physical_device.enumerateDeviceExtensionProperties();

        for (auto& available_extension : available_extensions) {
            if (std::string(extension) == available_extension.extensionName)
                return true;
        }

        return false;
    }

    static std::vector<std::string> CheckDeviceExtensionSupport(const vk::PhysicalDevice physical_device, std::span<const char * const> extensions) {
        std::vector<std::string> unsupported_extensions;
        const auto available_extensions = physical_device.enumerateDeviceExtensionProperties();
        for (auto& extension : extensions) {
            bool is_found = false;
            for (auto& available_extension : available_extensions) {
                if (std::string(extension) == available_extension.extensionName)
                    is_found = true;
            }
            if (is_found == false) {
                unsupported_extensions.emplace_back(extension);
            }
        }

        return unsupported_extensions;
    }

    static std::expected<SwapchainProperties, std::string> GetSupportedSwapchainProperties(vk::PhysicalDevice m_physical_device, vk::SurfaceKHR m_surface, Uint2 window_extent, bool vsync_on = true) {
        auto surface_capabilities = m_physical_device.getSurfaceCapabilitiesKHR(m_surface);
        auto surface_formats = m_physical_device.getSurfaceFormatsKHR(m_surface);
        auto surface_present_modes = m_physical_device.getSurfacePresentModesKHR(m_surface);

        const auto& choose = 
        []<typename T> 
        (std::vector<T> list, std::vector<T> preferred_list, T selected_null = {}) -> T {
            for (const auto desired : preferred_list) {
                if (auto it = std::ranges::find(list, desired);
                    it != list.end()) {

                    return desired;
                }
            }
            return selected_null;
        };

        SwapchainProperties out;
        //choose m_surface format
        {
            std::vector<vk::SurfaceFormatKHR> preferred_formats = {
                //{vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
                {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}
            };

            auto surface_format = choose(
                surface_formats, 
                preferred_formats,
                {vk::Format::eUndefined, vk::ColorSpaceKHR{}});
            
            if (surface_format.format == vk::Format::eUndefined) {
                std::vector<std::string> preferred_formats_str;

                for (auto& format : preferred_formats) {
                    preferred_formats_str.emplace_back(
                        std::format("format: {}, color space: {}", 
                            vk::to_string(format.format), vk::to_string(format.colorSpace)));
                }

                return std::unexpected(std::format("preferred m_surface formats not supported: preferred formats {}", preferred_formats_str));
            }

            out.format = surface_format.format;
            out.color_space = surface_format.colorSpace;
        }

        //choose present mode
        {
            std::vector<vk::PresentModeKHR> preferred_modes;
            if (vsync_on) {
                preferred_modes = {
                    vk::PresentModeKHR::eMailbox,
                    vk::PresentModeKHR::eFifo,
                    vk::PresentModeKHR::eFifoRelaxed,
                    vk::PresentModeKHR::eImmediate
                };
            }
            else {
                preferred_modes = {
                    vk::PresentModeKHR::eImmediate,
                    //some m_device don't support immediate
                    vk::PresentModeKHR::eFifo
                };
            }

            out.present_mode = choose(
                surface_present_modes,
                preferred_modes,
                vk::PresentModeKHR::eImmediate);

            if (vsync_on && out.present_mode == vk::PresentModeKHR::eImmediate) {
                std::vector<std::string> preferred_modes_str;

                for (auto& format : preferred_modes) {
                    preferred_modes_str.emplace_back(
                            vk::to_string(format));
                }
            }
        }

        //choose extent
        {
            out.extent.width = 
                std::clamp(uint32_t(window_extent.x), 
                surface_capabilities.minImageExtent.width, 
                surface_capabilities.maxImageExtent.width);

            out.extent.height = 
                std::clamp(uint32_t(window_extent.y), 
                surface_capabilities.minImageExtent.height, 
                surface_capabilities.maxImageExtent.height);
        }

        {
            constexpr auto preferred_image_count = 3u;
            out.image_count = std::clamp(preferred_image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount);
        }

        return out;
    }

    bool CheckPhysicalDeviceFeatures(vk::PhysicalDevice physical_device, Context::SupportedFeatures& supported_features, std::string& out_message) {
        vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
        vk::PhysicalDeviceMemoryPriorityFeaturesEXT memory_priorty{};
        vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_device_local_memory{};
        vk::PhysicalDeviceSynchronization2FeaturesKHR sync2{};
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{};
        vk::PhysicalDeviceVulkan11Features features11{};
        vk::PhysicalDeviceFeatures2 features{};

        memory_priorty.setPNext(&dynamic_rendering);
        pageable_device_local_memory.setPNext(&memory_priorty);
        sync2.setPNext(&pageable_device_local_memory);
        descriptor_indexing.setPNext(&sync2);
        features11.setPNext(&descriptor_indexing);
        features.setPNext(&features11);
        physical_device.getFeatures2(&features);

        if (!features.features.robustBufferAccess) {
            out_message = "robustBufferAccess feature not supported"; 
            return false;
        }

        if (!features11.shaderDrawParameters) {
            out_message = "shaderDrawParameters feature not supported";
            return false;
        }

        if (const auto unsupported_exts = CheckDeviceExtensionSupport(physical_device, required_extensions);
            !unsupported_exts.empty()) {
            out_message = std::format("your gpu dont support this extensions: {}", unsupported_exts);
            return false;
        }

        supported_features.sampled_image_update_after_bind
            = descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind;

        supported_features.storage_image_update_after_bind
            = descriptor_indexing.descriptorBindingStorageImageUpdateAfterBind;

        supported_features.uniform_buffer_update_after_bind
            = descriptor_indexing.descriptorBindingUniformBufferUpdateAfterBind;

        supported_features.storage_buffer_update_after_bind
            = descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind;

        supported_features.sync2
            = sync2.synchronization2;

        supported_features.pageable_device_local_memory
            = pageable_device_local_memory.pageableDeviceLocalMemory;
        
        supported_features.memory_priority
            = memory_priorty.memoryPriority;

        supported_features.anisotropy
            = features.features.samplerAnisotropy;

        supported_features.dynamic_rendering
            = dynamic_rendering.dynamicRendering;

        supported_features.memory_budget
            = CheckDeviceExtensionSupport(physical_device, "VK_EXT_memory_budget");

        return true;
    }
    
    Context::~Context() {
        if (m_instance == VK_NULL_HANDLE) {
            return;
        }

        if (m_device) m_device.waitIdle();
        
        if (m_empty_set_layout) m_device.destroy(m_empty_set_layout);
        if (m_depth_buffer) delete m_depth_buffer;
        if (m_resolve_image) delete m_resolve_image;
        if (placeholder_image) delete placeholder_image;
        if (placeholder_sampler) delete placeholder_sampler;
        if (m_command_buffer) delete m_command_buffer;
        
        DeleteVulkanObjects();
        
        if (m_vma_allocator) vmaDestroyAllocator(m_vma_allocator);

        for (const auto& framebuffers : m_framebuffers | std::ranges::views::values) {
            for (const auto framebuffer : framebuffers) {
                m_device.destroy(framebuffer);    
            }
        }
        
        for (const auto image_view : m_swapchain_image_views) {
            m_device.destroy(image_view);    
        }
        
        if (m_pipeline_cache) m_device.destroy(m_pipeline_cache);
        if (m_descriptor_pool) m_device.destroy(m_descriptor_pool);
        if (m_command_pool) m_device.destroy(m_command_pool);
        if (m_swapchain) m_device.destroy(m_swapchain);
        if (m_fence) m_device.destroy(m_fence);
        if (m_acquire_next_image_semaphore) m_device.destroy(m_acquire_next_image_semaphore);
        if (m_render_finished_semaphore) m_device.destroy(m_render_finished_semaphore);
        if (m_device) m_device.destroy();
        
        if (m_surface) m_instance.destroy(m_surface);
        if constexpr (_debug) {
            m_instance.destroy(m_debug_messanger, {}, dispatcher);
        }
        m_instance.destroy();
    }
    
    void Context::IInit(const ContextDesc& desc) {
        m_swapchain_settings = desc.swapchain_settings;
        shader_directory = desc.shader_directory;
        CreateInstance(GetWindowType(desc.window_handle));
        if constexpr (_debug) CreateDebugMessenger();
        CreateSurface(desc.window_handle);
        CreateDevice();
        CreateCommandPool();
        CreateDescriptorPool();
        CreateSwapchain(desc.swapchain_settings.window_extent, desc.swapchain_settings.Vsync);
        CreateVmaAllocator();
        CreatePipelineCache();
        CreateResource();
    }

    void Context::CreateInstance(WindowType window_type) {
        std::vector<const char*> extensions;
        extensions.emplace_back("VK_KHR_surface");

        if constexpr (_os == OS::eWin) {
            extensions.emplace_back("VK_KHR_win32_surface");
        }
        else if constexpr (_os == OS::eAndroid) {
            extensions.emplace_back("VK_KHR_android_surface");
        }
        else if constexpr (_os == OS::eLinux) {
            DnmGLAssert((window_type == WindowType::eWayland) || (window_type == WindowType::eX11), "unsupported window manager")

            if (window_type == WindowType::eWayland) {
                extensions.emplace_back("VK_KHR_wayland_surface");
            }
            else if (window_type == WindowType::eX11) {
                extensions.emplace_back("VK_KHR_xlib_surface");
            }
        }
        else {
            static_assert((_os != OS::eMac) || (_os != OS::eIos), "mac and ios don't support yet");
        }

        if constexpr (_debug) extensions.emplace_back("VK_EXT_debug_utils");
    
        std::vector<const char*> layers{};
        if constexpr (_debug) layers.emplace_back("VK_LAYER_KHRONOS_validation");
        if constexpr (_debug) layers.emplace_back("VK_LAYER_KHRONOS_synchronization2");
    
        vk::ApplicationInfo application_info{};
        application_info.setApplicationVersion(VK_MAKE_VERSION(0, 1, 0))
                        .setApiVersion(VK_API_VERSION_1_1)
                        .setEngineVersion(VK_MAKE_VERSION(0, 1, 0))
                        .setPEngineName("DnmGL")
                        .setPApplicationName("dnm");

        std::vector<vk::ValidationFeatureEnableEXT> enabled_features = {
            vk::ValidationFeatureEnableEXT::eBestPractices,
            vk::ValidationFeatureEnableEXT::eSynchronizationValidation
        };

        vk::ValidationFeaturesEXT validataion_features{};
        validataion_features.setEnabledValidationFeatures(enabled_features);

        vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{};
        debug_create_info.setMessageSeverity(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning 
                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
            .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            .setPfnUserCallback(DebugCallback)
            .setPNext(&validataion_features)
            ;
        
        vk::InstanceCreateInfo instance_create_info;
        instance_create_info.setPEnabledExtensionNames(extensions)
        .setPEnabledLayerNames(layers)
        .setPApplicationInfo(&application_info);

        if constexpr (_debug) instance_create_info.setPNext(&debug_create_info); 

        m_instance = vk::createInstance(instance_create_info);
    
        {
            DISPATCH_VK_FUNC(vkCreateDebugUtilsMessengerEXT);
            DISPATCH_VK_FUNC(vkDestroyDebugUtilsMessengerEXT);
            DISPATCH_VK_FUNC(vkCmdPipelineBarrier2KHR);
            DISPATCH_VK_FUNC(vkCmdBeginRenderingKHR);
            DISPATCH_VK_FUNC(vkCmdEndRenderingKHR);
        }
    }
    
    void Context::CreateDebugMessenger() {  
        vk::DebugUtilsMessengerCreateInfoEXT create_info(
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            DebugCallback,
            nullptr
        );
        m_debug_messanger = m_instance.createDebugUtilsMessengerEXT(create_info, nullptr, dispatcher);
    }
    
    void Context::CreateSurface(const WindowHandle& window_handle) {
        auto win_handle = std::get<WinWindowHandle>(window_handle);

        vk::Win32SurfaceCreateInfoKHR create_info{};
        create_info.setHinstance(reinterpret_cast<HINSTANCE>(win_handle.hInstance));
        create_info.setHwnd(reinterpret_cast<HWND>(win_handle.hwnd));

        vkCreateWin32SurfaceKHR(
            m_instance, 
            (VkWin32SurfaceCreateInfoKHR*)&create_info, 
            {},
            (VkSurfaceKHR*)&m_surface);
    }
    
    void Context::CreateDevice() {
        std::vector<const char*> extensions(required_extensions);

        if constexpr (_os == OS::eAndroid) 
            extensions.emplace_back("VK_ANDROID_external_memory_android_hardware_buffer");
    
        auto physics_devices = m_instance.enumeratePhysicalDevices();
        
        if (physics_devices.empty())
            Message("failed to find GPUs with Vulkan support!", MessageType::eUnsupportedDevice);
        
        std::string out;
        
        // TODO: choose best gpu
        for (auto& _device : physics_devices) { 
            if (!CheckPhysicalDeviceFeatures(_device, supported_features, out))
                continue;

            m_physical_device = _device;
        }
        
        if (m_physical_device == VK_NULL_HANDLE) {
            Message(std::format("No suitable GPU found: {}", out), MessageType::eUnsupportedDevice);
        }

        vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
        vk::PhysicalDeviceMemoryPriorityFeaturesEXT memory_priorty{};
        vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_device_local_memory{};
        vk::PhysicalDeviceSynchronization2FeaturesKHR sync2{};
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{};
        vk::PhysicalDeviceVulkan11Features features11{};
        vk::PhysicalDeviceFeatures2 features{};

        memory_priorty.setPNext(&dynamic_rendering);
        pageable_device_local_memory.setPNext(&memory_priorty);
        sync2.setPNext(&pageable_device_local_memory);
        descriptor_indexing.setPNext(&sync2);
        features11.setPNext(&descriptor_indexing);
        features.setPNext(&features11);

        features11.shaderDrawParameters = vk::True;
        features.features.robustBufferAccess = vk::True;
        features.features.samplerAnisotropy = features.features.samplerAnisotropy;
        descriptor_indexing.descriptorBindingUniformBufferUpdateAfterBind = supported_features.uniform_buffer_update_after_bind;
        descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind = supported_features.storage_buffer_update_after_bind;
        descriptor_indexing.descriptorBindingStorageImageUpdateAfterBind = supported_features.storage_image_update_after_bind;
        descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind = supported_features.sampled_image_update_after_bind;
        memory_priorty.memoryPriority = supported_features.memory_priority;
        pageable_device_local_memory.pageableDeviceLocalMemory = supported_features.pageable_device_local_memory;
        sync2.synchronization2 = supported_features.sync2;
        dynamic_rendering.dynamicRendering = supported_features.dynamic_rendering;

        if (supported_features.sync2) {
            extensions.emplace_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        }
        if (supported_features.uniform_buffer_update_after_bind
        || supported_features.storage_buffer_update_after_bind
        || supported_features.storage_image_update_after_bind
        || supported_features.sampled_image_update_after_bind) {
            extensions.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        }
        if (supported_features.memory_priority) {
            extensions.emplace_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
        }
        if (supported_features.memory_budget) {
            extensions.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        }
        if (supported_features.pageable_device_local_memory) {
            extensions.emplace_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
        }
        if (supported_features.dynamic_rendering) {
            extensions.emplace_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }

        Message(std::format("{}", std::string(supported_features)), MessageType::eInfo);
    
        uint32_t queue_family_index = 0;
        for (auto queue_family : m_physical_device.getQueueFamilyProperties()) {
            if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
                device_features.queue_flags = queue_family.queueFlags;
                device_features.timestamp_valid_bits = queue_family.timestampValidBits;
                break;
            }
    
            queue_family_index++;
        }
        device_features.queue_family = queue_family_index;
    
        float queue_priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_create_info;
        queue_create_info.setPQueuePriorities(&queue_priority)
        .setQueueFamilyIndex(device_features.queue_family)
        .setQueueCount(1);
    
        vk::DeviceCreateInfo deviceCreateInfo;
        deviceCreateInfo.setQueueCreateInfos(queue_create_info)
                        .setEnabledExtensionCount(extensions.size())
                        .setPEnabledExtensionNames(extensions)
                        .setPNext(&features);
    
        m_device = m_physical_device.createDevice(deviceCreateInfo);
        
        m_queue = m_device.getQueue(device_features.queue_family, 0);    
        m_fence = m_device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        m_acquire_next_image_semaphore = m_device.createSemaphore({});
        m_render_finished_semaphore = m_device.createSemaphore({});
    }
    
    void Context::CreateCommandPool() {
        vk::CommandPoolCreateInfo create_info{};
        create_info.setQueueFamilyIndex(device_features.queue_family);
    
        m_command_pool = m_device.createCommandPool(create_info);
    
        m_command_buffer = new CommandBuffer(*this);
    }

    void Context::CreateDescriptorPool() {
        vk::DescriptorPoolSize pool_sizes[4];
        pool_sizes[0].setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(512);
        pool_sizes[1].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(512);
        pool_sizes[2].setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(512);
        pool_sizes[3].setType(vk::DescriptorType::eStorageImage).setDescriptorCount(512);

        m_descriptor_pool = m_device.createDescriptorPool(
            vk::DescriptorPoolCreateInfo{}
                .setFlags({})
                .setMaxSets(512)
                .setPoolSizes(pool_sizes));
    }
    
    void Context::CreateSwapchain(Uint2 extent, bool Vsync) {
        m_swapchain_properties = GetSupportedSwapchainProperties(m_physical_device, m_surface, extent, Vsync).or_else(
            [this] (auto error_str) -> std::expected<SwapchainProperties, std::string> {
                Message(error_str, MessageType::eUnsupportedDevice);
                return SwapchainProperties{};
            }
        ).value();
        std::println("{}", std::string(m_swapchain_properties));
    
        vk::SwapchainCreateInfoKHR create_info{};
        create_info.setOldSwapchain(m_swapchain)
                    .setImageFormat(m_swapchain_properties.format)
                    .setImageColorSpace(m_swapchain_properties.color_space)
                    .setImageArrayLayers(1)
                    .setImageExtent(m_swapchain_properties.extent)
                    .setMinImageCount(m_swapchain_properties.image_count)
                    .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
                    .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
                    .setImageSharingMode(vk::SharingMode::eExclusive)
                    .setQueueFamilyIndices({device_features.queue_family})
                    .setClipped(vk::True)
                    .setSurface(m_surface)
                    .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                    .setPresentMode(m_swapchain_properties.present_mode)
                    ;

        m_swapchain = m_device.createSwapchainKHR(create_info);
    
        m_swapchain_images = m_device.getSwapchainImagesKHR(m_swapchain);
    
        m_swapchain_image_views.resize(m_swapchain_images.size());
    
        uint32_t i = 0;
        for (auto image : m_swapchain_images) {
            vk::ImageViewCreateInfo image_view_create_info{};
            image_view_create_info.setImage(image)
                                    .setViewType(vk::ImageViewType::e2D)
                                    .setFormat(m_swapchain_properties.format)
                                    .setComponents(vk::ComponentMapping{})
                                    .setSubresourceRange(vk::ImageSubresourceRange()
                                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                        .setLayerCount(1)
                                        .setBaseArrayLayer(0)
                                        .setBaseMipLevel(0)
                                        .setLevelCount(1))
                                        ;
            m_swapchain_image_views[i] = m_device.createImageView(image_view_create_info);
            ++i;
        }
    }
    
    void Context::CreateVmaAllocator() {
        VmaAllocatorCreateFlags create_flag_bits
            = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
        if (supported_features.memory_budget) {
            create_flag_bits |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }
        if (supported_features.memory_priority) {
            create_flag_bits |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        }
    
        VmaAllocatorCreateInfo createInfo{};
        createInfo.instance = m_instance;
        createInfo.device = m_device;
        createInfo.physicalDevice = m_physical_device;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_1;
        createInfo.flags = create_flag_bits;
    
        const auto result = (vk::Result)vmaCreateAllocator(&createInfo, &m_vma_allocator);
    
        if (result != vk::Result::eSuccess)
            Message(std::format(
                "vma allocator failed to create, Error: {}", 
                vk::to_string(static_cast<vk::Result>(result))), 
                MessageType::eGraphicsBackendInternal);
    }
    
    void Context::CreatePipelineCache() {
        m_pipeline_cache = m_device.createPipelineCache({});
    }

    void Context::ExecuteCommands(const std::function<bool(DnmGL::CommandBuffer*)>& func) {
        [[maybe_unused]] auto _ = m_device.waitForFences(m_fence, vk::True, 1'000'000'000);
        m_device.resetFences(m_fence);

        ProcessResourceUpdates();
        DeleteVulkanObjects();

        m_device.resetCommandPool(m_command_pool);
        m_command_buffer->command_buffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        context_state = ContextState::eCommandBufferRecording;
        if (!func(m_command_buffer)) {
            m_command_buffer->End();
            return;
        }
        m_command_buffer->End();
    
        const vk::SubmitInfo submit_info(
            {},
            {},
            {},
            1,
            &m_command_buffer->command_buffer,
            0,
            nullptr,
            {}
        );

        m_queue.submit({submit_info}, m_fence);
        context_state = ContextState::eCommandExecuting;
    }

    void Context::Render(const std::function<bool(DnmGL::CommandBuffer*)>& func) {
        [[maybe_unused]] auto _ = m_device.waitForFences(m_fence, vk::True, 1'000'000'000);
        m_device.resetFences(m_fence);

        //get the next image
        {
            const auto result 
                = m_device.acquireNextImageKHR(m_swapchain, 1'000'000'000, m_acquire_next_image_semaphore, nullptr);

            if (result.result == vk::Result::eErrorDeviceLost) {
                // TODO: make better log
                Message("aquiring swapchain image in device lost", MessageType::eDeviceLost);
                m_device.waitIdle();
            }
            else if (result.result == vk::Result::eErrorOutOfDateKHR || result.result == vk::Result::eSuboptimalKHR) {
                // TODO: make better log
                Message("swapchain is not ideal", MessageType::eOutOfDateSwapchain);
            } 
            else if (result.result != vk::Result::eSuccess) {
                Message("failed to aquiring swapchain image", MessageType::eUnknown);
            }

            m_image_index = result.value;
        }

        ProcessResourceUpdates();
        DeleteVulkanObjects();

        {
            m_device.resetCommandPool(m_command_pool);
            m_command_buffer->command_buffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            context_state = ContextState::eCommandBufferRecording;
            if (!func(m_command_buffer)) {
                m_command_buffer->End();
                return;
            }
            m_command_buffer->End();

            constexpr vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        
            const vk::SubmitInfo submit_info(
                1,
                &m_acquire_next_image_semaphore,
                &wait_stage,
                1,
                &m_command_buffer->command_buffer,
                1,
                &m_render_finished_semaphore,
                {}
            );
        
            m_queue.submit({submit_info}, m_fence);
        }

        //Present image
        {
            const vk::PresentInfoKHR present_info(
                1,
                &m_render_finished_semaphore,
                1,
                &m_swapchain,
                &m_image_index
            );
    
            const auto result = m_queue.presentKHR(present_info);
    
            if (result == vk::Result::eErrorDeviceLost) {
                // TODO: make better log
                Message("presenting in device lost", MessageType::eDeviceLost);
                m_device.waitIdle();
            }
            else if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
                // TODO: make better log
                Message("swapchain is not ideal", MessageType::eOutOfDateSwapchain);
            } 
            else if (result != vk::Result::eSuccess) {
                Message("failed to presenting", MessageType::eUnknown);
            }
        }
        context_state = ContextState::eCommandExecuting;
    }

    void Context::ProcessResourceUpdates() {
        std::vector<vk::WriteDescriptorSet> writes{};
        std::vector<vk::DescriptorImageInfo> image_infos{};
        std::vector<vk::DescriptorBufferInfo> buffer_infos{};

        writes.reserve(defer_resource_update.size());
        image_infos.reserve(defer_resource_update.size());
        buffer_infos.reserve(defer_resource_update.size());

        for (const auto& descriptor : defer_resource_update) {
            std::visit([&writes, &image_infos, &buffer_infos, this] (auto&& res) {
                using T = std::decay_t<decltype(res)>;
                if constexpr (std::is_same_v<T, InternalBufferResource>) {
                    ProcessResource(res, buffer_infos.emplace_back(), writes.emplace_back());
                }
                else if constexpr (std::is_same_v<T, InternalImageResource>) {
                    ProcessResource(res, image_infos.emplace_back(), writes.emplace_back());
                }
                else if constexpr (std::is_same_v<T, InternalSamplerResource>) {
                    ProcessResource(res, image_infos.emplace_back(), writes.emplace_back());
                }
            }, descriptor);
        }

        m_device.updateDescriptorSets(writes, {});

        defer_resource_update.clear();
    }

    void Context::CreateResource() {
        m_empty_set_layout = m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{}.setBindingCount(0));
        m_empty_set
            = m_device.allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo{}
                    .setSetLayouts(m_empty_set_layout)
                    .setDescriptorSetCount(1)
                    .setDescriptorPool(m_descriptor_pool))[0];

        ExecuteCommands([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
            auto *typed_command_buffer = static_cast<Vulkan::CommandBuffer*>(command_buffer);

            typed_command_buffer->BeginCopyPass();

            placeholder_image = new DnmGL::Vulkan::Image(*this, {
                .extent = {1, 1, 1},
                .format = DnmGL::ImageFormat::eRGBA8Norm,
                .usage_flags = DnmGL::ImageUsageBits::eReadonlyResource,
                .type = DnmGL::ImageType::e2D,
                .mipmap_levels = 1,
            });

            CreateDepthBuffer(
                m_swapchain_settings.window_extent, 
                m_swapchain_settings.msaa, 
                m_swapchain_settings.depth_buffer_format);

            CreateResolveImage(
                m_swapchain_settings.window_extent, 
                m_swapchain_settings.msaa);

            const uint32_t pixel_data = -1;
            typed_command_buffer->UploadData<uint32_t>(
                placeholder_image,
                ImageSubresource{},
                std::span(&pixel_data, 1),
                {1,1,1}, 
                {0,0,0}
            );
            return true;
        });

        placeholder_sampler = new DnmGL::Vulkan::Sampler(*this, {
            .compare_op = DnmGL::CompareOp::eNever,
            .filter = DnmGL::SamplerFilter::eNearest
        });
    }

    vk::Framebuffer Context::GetOrCreateFramebuffer(vk::RenderPass renderpass) noexcept {
        auto [it, emplaced] = m_framebuffers.try_emplace(renderpass, m_swapchain_properties.image_count);
        if (emplaced) {
            CreateFramebuffers(renderpass, it->second);
        }
    
        return it->second[m_image_index];
    }

    void Context::ISetSwapchainSettings(const SwapchainSettings& new_settings) {
        const bool Vsync_change = m_swapchain_settings.Vsync != new_settings.Vsync;
        const bool extent_change = m_swapchain_settings.window_extent != new_settings.window_extent;
        const bool msaa_change = m_swapchain_settings.msaa != new_settings.msaa;
        const bool depth_format_change = m_swapchain_settings.depth_buffer_format != new_settings.depth_buffer_format;

        m_swapchain_settings = new_settings;

        if (!(Vsync_change || extent_change || msaa_change || depth_format_change))
            return;

        if (Vsync_change || extent_change)
            CreateSwapchain(new_settings.window_extent, new_settings.Vsync);

        //for image layout translating
        if (extent_change || msaa_change || depth_format_change)
            ExecuteCommands([&] (DnmGL::CommandBuffer *) -> bool {
                CreateDepthBuffer(new_settings.window_extent, 
                    new_settings.msaa, 
                    new_settings.depth_buffer_format);
        
                if (msaa_change) {
                    CreateResolveImage(new_settings.window_extent, new_settings.msaa);
                }
                return true;
            });

        ReCreateFramebuffersForSwapchainChanges();
    }

    void Context::CreateDepthBuffer(Uint2 extent, SampleCount sample_count, ImageFormat format) {
        if (format == ImageFormat::eUndefined) {
            delete m_depth_buffer;
            m_depth_buffer = nullptr;
            return;
        }

        m_depth_buffer = new Vulkan::Image(*this, {
            .extent = {extent, 1},
            .format = format,
            .usage_flags = DnmGL::ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment,
            .type = DnmGL::ImageType::e2D,
            .mipmap_levels = 1,
            .sample_count = sample_count
        });
    }

    void Context::CreateResolveImage(Uint2 extent, SampleCount sample_count) {
        if (sample_count == SampleCount::e1) {
            delete m_resolve_image;
            m_resolve_image = nullptr;
            return;
        }

        m_resolve_image = new Vulkan::Image(*this, {
            .extent = {extent, 1},
            .format = ImageFormat::eRGBA8Norm,
            .usage_flags = DnmGL::ImageUsageBits::eColorAttachment | ImageUsageBits::eTransientAttachment,
            .type = DnmGL::ImageType::e2D,
            .mipmap_levels = 1,
            .sample_count = sample_count,
        });
    }

    void Context::ReCreateFramebuffersForSwapchainChanges() noexcept {
        if (GetSupportedFeatures().dynamic_rendering)
            return;

        for (auto &[renderpass, framebuffer] : m_framebuffers) {
            CreateFramebuffers(renderpass, framebuffer);
        }
    }

    void Context::CreateFramebuffers(vk::RenderPass renderpass, std::vector<vk::Framebuffer>& out_framebuffers) noexcept {
        out_framebuffers.resize(m_swapchain_properties.image_count);

        vk::FramebufferCreateInfo framebuffer_info{};
        framebuffer_info.setAttachments(0)
                        .setWidth(m_swapchain_properties.extent.width)
                        .setHeight(m_swapchain_properties.extent.height)
                        .setLayers(1)
                        .setRenderPass(renderpass);
                        ;

        for (const auto i : Counter(m_swapchain_properties.image_count)) {
            std::vector<vk::ImageView> attachments;
            attachments.emplace_back(m_swapchain_image_views[i]);
            if (m_resolve_image) attachments.emplace_back(m_resolve_image->CreateGetImageView({}));
            attachments.emplace_back(m_depth_buffer->CreateGetImageView({}));  

            framebuffer_info.setAttachments(attachments);
            out_framebuffers[i] = m_device.createFramebuffer(framebuffer_info);
        }   
    }

    std::unique_ptr<DnmGL::Buffer> Context::CreateBuffer(const DnmGL::BufferDesc& desc) noexcept {
        return std::make_unique<DnmGL::Vulkan::Buffer>(*this, desc);
    }

    std::unique_ptr<DnmGL::Image> Context::CreateImage(const DnmGL::ImageDesc& desc) noexcept {
        return std::make_unique<DnmGL::Vulkan::Image>(*this, desc);
    }

    std::unique_ptr<DnmGL::Sampler> Context::CreateSampler(const DnmGL::SamplerDesc& desc) noexcept {
        return std::make_unique<DnmGL::Vulkan::Sampler>(*this, desc);
    }

    std::unique_ptr<DnmGL::Shader> Context::CreateShader(std::string_view filename) noexcept {
        return std::make_unique<DnmGL::Vulkan::Shader>(*this, filename);
    }

    std::unique_ptr<DnmGL::ResourceManager> Context::CreateResourceManager(std::span<const DnmGL::Shader*> shaders) noexcept {
        return std::make_unique<DnmGL::Vulkan::ResourceManager>(*this, shaders);
    }

    std::unique_ptr<DnmGL::ComputePipeline> Context::CreateComputePipeline(const DnmGL::ComputePipelineDesc& desc) noexcept {
        return std::make_unique<DnmGL::Vulkan::ComputePipeline>(*this, desc);
    }

    std::unique_ptr<DnmGL::GraphicsPipeline> Context::CreateGraphicsPipeline(const DnmGL::GraphicsPipelineDesc& desc) noexcept {
        if (GetSupportedFeatures().dynamic_rendering) {
            return std::make_unique<DnmGL::Vulkan::GraphicsPipelineDynamicRendering>(*this, desc);
        }
        return std::make_unique<DnmGL::Vulkan::GraphicsPipelineDefaultVk>(*this, desc);
    }

    std::unique_ptr<DnmGL::Framebuffer> Context::CreateFramebuffer(const DnmGL::FramebufferDesc& desc) noexcept {
        if (GetSupportedFeatures().dynamic_rendering) {
            return std::make_unique<DnmGL::Vulkan::FramebufferDynamicRendering>(*this, desc);
        }
        return std::make_unique<DnmGL::Vulkan::FramebufferDefaultVk>(*this, desc);
    }
} // namespace DnmGL::Vulkan