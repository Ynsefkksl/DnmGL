#include "DnmGL/DnmGL.hpp"

#ifdef DnmGL_D3D12_Backend
    #include "DnmGL/D3D12/Context.hpp"

    extern "C" DNMGL_API DnmGL::Context *CreateD3D12Context() {
        return new DnmGL::D3D12::Context();
    }
#else
    //TODO: maybe has error message
    extern "C" DNMGL_API DnmGL::Context *CreateD3D12Context() {
        return nullptr;
    }
#endif

#ifdef DnmGL_Vulkan_Backend
    #include "DnmGL/Vulkan/Context.hpp"

    extern "C" DNMGL_API DnmGL::Context *CreateVulkanContext() {
        return new DnmGL::Vulkan::Context();
    }
#else
    //TODO: maybe has error message
    extern "C" DNMGL_API DnmGL::Context *CreateVulkanContext() {
        return nullptr;
    }
#endif

#ifdef DnmGL_Metal_Backend
    #include "DnmGL/Metal/Context.hpp"

    extern "C" DNMGL_API DnmGL::Context *CreateMetalContext() {
        return new DnmGL::Metal::Context();
    }
#else
    //TODO: maybe has error message
    extern "C" DNMGL_API DnmGL::Context *CreateMetalContext() {
        return nullptr;
    }
#endif