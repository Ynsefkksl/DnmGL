#pragma once

#include "DnmGL/Utility/Counter.hpp"
#include "DnmGL/Utility/Macros.hpp"
#include "DnmGL/Utility/Flag.hpp"
#include "DnmGL/Utility/Math.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <format>
#include <span>
#include <stdexcept>
#include <variant>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <source_location>
#include <ranges>
#include <map>
#include <set>

//TODO: add vulkan supported feature override
//TODO: add per-frame buffer (vulkan dynamic buffer like something)
//TODO: add push constant support (or alternative something)
//TODO: Vertex and Index buffers broken, pwease fix dis fow pwe-Turing’ GPUs (๑˃ᴗ˂)ﻭ ♡
//TODO: D3D12 copy alignment problem
//TODO: complete d3d12 generate mipmap function
//TODO: stencil is broken
//TODO: add compressed image format (ktx)
//TODO: add for d3d12 draw between the sync (use enhanced barrier if not supported track uav resources)
//TODO: check subresources

namespace DnmGL {
    class Context;
    class CommandBuffer;
    class Buffer;
    class Image;
    class Sampler;
    class ComputePipeline;
    class GraphicsPipeline;
    class ResourceManager;
    class Framebuffer;
    class ShaderCompiler;

    template <class... Types> 
    constexpr void DnmGLAssertFunc(std::string_view func_name, std::string_view condition_str, bool condition, const std::format_string<Types...> fmt, Types&&... args) {
        if (condition) {
            return;
        }
        std::println("[DnmGL Error ({})] Condition: {}, Message: {}", func_name, condition_str, std::vformat(fmt.get(), std::make_format_args(args...)));
        exit(1);
    }

    #define DnmGLAssert(condition, fmt, ...) \
        DnmGL::DnmGLAssertFunc(std::source_location::current().function_name(), #condition, condition, fmt, __VA_ARGS__);

    //just stencil type not supported in d3d12
    //16d 8s type not supported in metal
    //rgb types not supported in metal
    enum class ImageFormat : uint8_t {
        eUndefined = 0,
        eR8UInt,
        eR8Norm,
        eR8SNorm,
        eR8SInt,
        eRG8UInt,
        eRG8Norm,
        eRG8SNorm,
        eRG8SInt,
        eRGBA8UInt,
        eRGBA8Norm,
        eRGBA8SNorm,
        eRGBA8SInt,
        eRGBA8Srgb,
        eR16UInt,
        eR16Float,
        eR16Norm,
        eR16SNorm,
        eR16SInt,
        eRG16UInt,
        eRG16Float,
        eRG16Norm,
        eRG16SNorm,
        eRG16SInt,
        eRGBA16UInt,
        eRGBA16Float,
        eRGBA16Norm,
        eRGBA16SNorm,
        eRGBA16SInt,
        eR32UInt,
        eR32Float,
        eR32SInt,
        eRG32UInt,
        eRG32Float,
        eRG32SInt,
        eRGBA32UInt,
        eRGBA32Float,
        eRGBA32SInt,
        // TODO: eB8G8R8A8Norm
        eD16Norm,
        eD32Float,
        eD24NormS8UInt,
        eD32NormS8UInt,
    };
    
    constexpr uint8_t GetFormatSize(const ImageFormat format) noexcept {
        switch (format) {
            case ImageFormat::eUndefined: return 0;
            case ImageFormat::eR8UInt:
            case ImageFormat::eR8Norm:
            case ImageFormat::eR8SNorm:
            case ImageFormat::eR8SInt: return 1;
            case ImageFormat::eRG8UInt:
            case ImageFormat::eRG8Norm:
            case ImageFormat::eRG8SNorm:
            case ImageFormat::eR16UInt:
            case ImageFormat::eR16Float:
            case ImageFormat::eR16Norm:
            case ImageFormat::eR16SNorm:
            case ImageFormat::eR16SInt:
            case ImageFormat::eD16Norm:
            case ImageFormat::eRG8SInt: return 2;
            case ImageFormat::eRGBA8UInt:
            case ImageFormat::eRGBA8Norm:
            case ImageFormat::eRGBA8SNorm:
            case ImageFormat::eRGBA8SInt:
            case ImageFormat::eRGBA8Srgb:
            case ImageFormat::eRG16UInt:
            case ImageFormat::eRG16Float:
            case ImageFormat::eRG16Norm:
            case ImageFormat::eRG16SNorm:
            case ImageFormat::eR32UInt:
            case ImageFormat::eR32Float:
            case ImageFormat::eR32SInt:
            case ImageFormat::eD32Float:
            case ImageFormat::eD24NormS8UInt:
            case ImageFormat::eRG16SInt: return 4;
            case ImageFormat::eRGBA16UInt:
            case ImageFormat::eRGBA16Float:
            case ImageFormat::eRGBA16Norm:
            case ImageFormat::eRGBA16SNorm:
            case ImageFormat::eRGBA16SInt:
            case ImageFormat::eRG32UInt:
            case ImageFormat::eRG32Float:
            case ImageFormat::eD32NormS8UInt:
            case ImageFormat::eRG32SInt: return 8;
            case ImageFormat::eRGBA32UInt:
            case ImageFormat::eRGBA32Float:
            case ImageFormat::eRGBA32SInt: return 16;
        }
        return 0;
    }

    //metal has separate format
    enum class VertexFormat : uint8_t {
        eUndefined = 0,
        e8UInt1,
        e8UInt2,
        e8UInt4,
        e8SInt1,
        e8SInt2,
        e8SInt4,
        e8Norm1,
        e8Norm2,
        e8Norm4,
        e8SNorm1,
        e8SNorm2,
        e8SNorm4,
        e16UInt1,
        e16UInt2,
        e16UInt4,
        e16SInt1,
        e16SInt2,
        e16SInt4,
        e16Norm1,
        e16Norm2,
        e16Norm4,
        e16SNorm1,
        e16SNorm2,
        e16SNorm4,
        e16Float1,
        e16Float2,
        e16Float4,
        e32UInt1,
        e32UInt2,
        e32UInt3,
        e32UInt4,
        e32SInt1,
        e32SInt2,
        e32SInt3,
        e32SInt4,
        e32Float1,
        e32Float2,
        e32Float3,
        e32Float4,
    };

    constexpr uint8_t GetFormatSize(const VertexFormat format) noexcept {
        switch (format) {
            case VertexFormat::eUndefined: return 0;
            case VertexFormat::e8UInt1:
            case VertexFormat::e8SInt1:
            case VertexFormat::e8Norm1:
            case VertexFormat::e8SNorm1: return 1;
            case VertexFormat::e8UInt2:
            case VertexFormat::e8Norm2:
            case VertexFormat::e8SNorm2:
            case VertexFormat::e8SInt2:
            case VertexFormat::e16UInt1:
            case VertexFormat::e16SInt1:
            case VertexFormat::e16Norm1:
            case VertexFormat::e16Float1:
            case VertexFormat::e16SNorm1: return 2;
            case VertexFormat::e8UInt4: 
            case VertexFormat::e8SInt4:
            case VertexFormat::e8Norm4:
            case VertexFormat::e8SNorm4:
            case VertexFormat::e16UInt2:
            case VertexFormat::e16SInt2:
            case VertexFormat::e16Norm2:
            case VertexFormat::e16SNorm2:
            case VertexFormat::e16Float2:
            case VertexFormat::e32UInt1:
            case VertexFormat::e32SInt1:
            case VertexFormat::e32Float1: return 4;
            case VertexFormat::e16UInt4:
            case VertexFormat::e16SInt4:
            case VertexFormat::e16Norm4:
            case VertexFormat::e16SNorm4:
            case VertexFormat::e16Float4:
            case VertexFormat::e32UInt2:
            case VertexFormat::e32Float2:
            case VertexFormat::e32SInt2: return 8;
            case VertexFormat::e32UInt3:
            case VertexFormat::e32SInt3:
            case VertexFormat::e32Float3: return 12;
            case VertexFormat::e32UInt4:
            case VertexFormat::e32SInt4:
            case VertexFormat::e32Float4: return 16;
        }
        return 0;
    }

    constexpr bool IsDepthFormat(const DnmGL::ImageFormat format) {
        return (format == DnmGL::ImageFormat::eD16Norm) || (format == DnmGL::ImageFormat::eD32Float);
    }

    constexpr bool IsDepthStencilFormat(const DnmGL::ImageFormat format) {
        return (format == DnmGL::ImageFormat::eD32NormS8UInt)
            || (format == DnmGL::ImageFormat::eD24NormS8UInt);
    }

    constexpr bool IsColorFormat(const DnmGL::ImageFormat format) {
        return !(IsDepthFormat(format) || IsDepthStencilFormat(format) || format == DnmGL::ImageFormat::eUndefined);
    }

    enum class ImageSubresourceType : uint8_t {
        e1D,
        e2D,
        e2DArray,
        e3D,
        eCube,
    };

    enum class ImageType : uint8_t {
        e1D,
        e2D,
        e3D,
    };

    enum class MemoryType : uint8_t {
        eAuto,  
        eHostMemory,
        eDeviceMemory,
    };

    enum class MemoryHostAccess : uint8_t {
        eNone,
        eReadWrite,
        eWrite
    };

    enum class CommandBufferPassType : uint8_t {
        eNone,
        eTransfer,
        eCompute,
        eRendering,
    };

    //same with vulkan
    enum class SamplerMipmapMode : uint8_t {
        eNearest,
        eLinear,
    };

    //same with vulkan
    enum class CompareOp : uint8_t {
        eNone,
        eNever,
        eLess,
        eEqual,
        eLessOrEqual,
        eGreater,
        eNotEqual,
        eGreaterOrEqual,
        eAlways
    };

    //same with vulkan
    enum class PolygonMode : uint8_t {
        eFill,
        eLine,
    };

    //same with vulkan
    enum class CullMode : uint8_t {
        eNone,
        eFront,
        eBack,
    };

    //same with vulkan
    enum class FrontFace : uint8_t {
        eCounterClockwise,
        eClockwise
    };

    enum class SamplerFilter : uint8_t {
        //same with vulkan
        eNearest,
        eLinear,

        //not same with vulkan
        //if don't supported uses linear
        eAnisotropyX2 = 2,
        eAnisotropyX4 = 4,
        eAnisotropyX8 = 8,
        eAnisotropyX16 = 16,
    };

    //same with vulkan
    enum class AttachmentLoadOp : uint8_t  {
        eLoad = 0,
        eClear,
        eDontCare
    };

    //same with vulkan
    enum class AttachmentStoreOp : uint8_t {
        eStore = 0,
        eDontCare
    };

    //same with vulkan
    enum class IndexType : uint8_t {
        eUint16,
        eUint32
    };

    //same with vulkan
    enum class PrimitiveTopology : uint8_t {
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
        eTriangleFan,
    };

    //same with vulkan
    enum class SamplerAddressMode : uint8_t {
        eRepeat,
        eMirroredRepeat,
        eClampToEdge,
        eClampToBorder
    };

    enum class ResourceType : uint8_t {
        eNone,
        eReadonlyBuffer,
        eReadonlyImage,
        eWritableBuffer,
        eWritableImage,
        eUniformBuffer,
        eSampler,
        //TODO: ePushConstant
    };

    enum class ContextState : uint8_t {
        eNone,
        eCommandBufferRecording,
        eCommandExecuting
    };

    //same with vulkan
    enum class SampleCount : uint8_t {
        e1 = 1 << 0,
        e2 = 1 << 1,
        e4 = 1 << 2,
        e8 = 1 << 3,
    };

    enum class BufferUsageBits : uint8_t {
        eUniform =  0x1,
        eWritableResource =  0x2,
        eReadonlyResource =  0x4,
        eVertex =   0x8,
        eIndex =    0x10,
        eIndirect = 0x20 //not implemented
    };
    using BufferUsageFlags = Flags<BufferUsageBits>;

    enum class ImageUsageBits : uint8_t {
        eReadonlyResource = 0x1,
        eWritableResource = 0x2,
        eColorAttachment = 0x4,
        eDepthStencilAttachment = 0x8,
        eTransientAttachment = 0x10
    };
    using ImageUsageFlags = Flags<ImageUsageBits>;

    enum class ShaderStageBits : uint8_t {
        eNone = 0x0,
        eVertex = 0x1,
        eFragment = 0x2,
        eCompute = 0x4,
    };
    using ShaderStageFlags = Flags<ShaderStageBits>;

    struct Color { uint8_t r, g, b, a; };
    struct ColorFloat { float r, g, b, a; };

    struct DepthStencilClearValue { 
        float depth; 
        uint32_t stencil; 
    };

    struct ImageDesc {
        //if type 2D and z > 1, z is array count 
        Uint3 extent = {1, 1, 1};
        ImageFormat format;
        ImageUsageFlags usage_flags;
        ImageType type;
        uint32_t mipmap_levels = 1;
        SampleCount sample_count = SampleCount::e1;
    };

    struct ImageSubresource {
        ImageSubresourceType type = ImageSubresourceType::e2D;
        uint8_t base_layer = 0;
        uint8_t base_mipmap = 0;
        uint8_t layer_count = 1;
        uint8_t mipmap_level = 1;

        auto operator<=>(const ImageSubresource &) const = default;
    };

    struct ImageSubresourceLayer {
        uint8_t base_layer = 0;
        uint8_t base_mipmap = 0;
        uint8_t layer_count = 1;
    };

    struct RenderAttachment {
        DnmGL::Image *image;
        ImageSubresource subresource;
    };

    struct RenderPassDesc {
        ImageFormat depth_stencil_format;
        AttachmentLoadOp depth_stencil_load_op;
        AttachmentStoreOp depth_stencil_store_op;
        std::vector<ImageFormat> color_formats;
        std::vector<AttachmentLoadOp> color_load_op;
        std::vector<AttachmentStoreOp> color_store_op;
    };

    struct SamplerDesc {
        SamplerMipmapMode mipmap_mode;
        CompareOp compare_op;
        SamplerFilter filter;
        SamplerAddressMode address_mode_u{};
        SamplerAddressMode address_mode_v{};
        SamplerAddressMode address_mode_w{};
    };

    struct FramebufferDesc {
        std::vector<ImageFormat> color_attachment_formats;
        ImageFormat depth_stencil_format;
        SampleCount msaa;
        Uint2 extent;
        //TODO: maybe i should have object create flags
        bool create_depth_buffer : 1;
    };

    struct BufferDesc {
        uint32_t element_size;
        uint32_t element_count;
        MemoryHostAccess memory_host_access;
        MemoryType memory_type;
        BufferUsageFlags usage_flags;
    };

    enum class ShaderType : uint8_t {
        eGraphicsShader,
        eComputeShader
    };

    struct Resource {
        ResourceType type;
        uint32_t spirv_index;
        uint32_t dxil_index;
        uint32_t metal_index;
        uint32_t resource_count;
        std::string name;
        std::variant<std::nullopt_t, BufferDesc, ImageDesc, SamplerDesc> create_desc = std::nullopt;
    };

    //TODO: add func input output
    struct EntryPoint {
        std::string name;
        ShaderStageBits shader_stage;
        std::vector<std::string> used_resources;
    };

    struct VertexBinding {
        VertexFormat vertex_format;
        uint32_t offset;
    };

    struct RasterizerDesc {
        std::vector<ImageFormat> color_attachment_formats{};
        PolygonMode polygone_mode{};
        CullMode cull_mode{};
        FrontFace front_face{};
        SampleCount msaa = SampleCount::e1;
        bool color_blend : 1{};
    };

    struct InputAssemblyDesc {
        std::vector<VertexBinding> vertex_bindings{};
        PrimitiveTopology topology{};
    };

    struct DepthStencilDesc {
        ImageFormat depth_stencil_format;
        CompareOp depth_test_compare_op;
        bool depth_test : 1;
        bool depth_write : 1;
        bool stencil_test : 1;
    };

    struct GraphicsPipelineDesc {
        std::string shader_name;
        RasterizerDesc *rasterizer_desc;
        InputAssemblyDesc *input_assembly_desc;
        DepthStencilDesc *depth_stencil_desc;
    };

    struct BufferToBufferCopyDesc {
        Buffer *src_buffer;
        Buffer *dst_buffer;
        uint32_t src_offset;
        uint32_t dst_offset;
        uint64_t copy_size;
    };

    //! is not subresource
    struct BufferToImageCopyDesc {
        Buffer *src_buffer;
        Image *dst_image;
        ImageSubresource image_subresource;
        uint32_t buffer_offset;
        Uint3 copy_offset;
        Uint3 copy_extent;
    };

    //! is not subresource
    struct ImageToImageCopyDesc {
        Image *src_image;
        Image *dst_image;
        ImageSubresource src_image_subresource;
        ImageSubresource dst_image_subresource;
        Uint3 src_offset;
        Uint3 dst_offset;
        Uint3 copy_extent;
    };

    //! is not subresource
    struct ImageToBufferCopyDesc {
        Image *src_image;
        Buffer *dst_buffer;
        ImageSubresource image_subresource;
        uint32_t buffer_offset;
        Uint3 copy_offset;
        Uint3 copy_extent;
    };

    struct AttachmentOps {
        DnmGL::AttachmentLoadOp color_load[8]{};
        DnmGL::AttachmentLoadOp depth_load{};
        DnmGL::AttachmentLoadOp stencil_load{};
        DnmGL::AttachmentStoreOp color_store[8]{};
        DnmGL::AttachmentStoreOp depth_store{};
        DnmGL::AttachmentStoreOp stencil_store{};

        //for just enjoy
        [[nodiscard]] constexpr uint32_t GetPacked(const bool presenting) const noexcept {
            uint32_t out{};
            out = static_cast<uint32_t>(depth_load) << 30;
            out |= static_cast<uint32_t>(stencil_load) << 28;
            for (uint32_t i{}; i < 8; i++) {
                out |= static_cast<uint32_t>(color_load[i]) << (26 - (i  *2));
            }
            out |= static_cast<uint32_t>(depth_store) << 11;
            out |= static_cast<uint32_t>(stencil_store) << 10;
            for (uint32_t i{}; i < 8; i++) {
                out |= static_cast<uint32_t>(color_store[i]) << (9 - (i));
            }
            out |= presenting << 1;
            return out;
        }
    };

    struct BeginRenderingDesc {
        std::span<const DnmGL::ColorFloat> color_clear_values;
        DnmGL::GraphicsPipeline *pipeline;
        DnmGL::Framebuffer *framebuffer;
        DnmGL::DepthStencilClearValue depth_stencil_clear_value;
        AttachmentOps attachment_ops;
    };

    struct BufferResourceDesc {
        DnmGL::Buffer *buffer;
        uint32_t first_element;
        uint32_t element_count;
    };

    struct ImageResourceDesc {
        DnmGL::Image *image;
        ImageSubresource subresource;
    };

    struct ShaderReflection {
        ShaderType shader_type;
        std::unordered_map<std::string, Resource> resources;
        std::unordered_map<std::string, EntryPoint> entry_points;
        std::optional<DepthStencilDesc> depth_stencil_desc;
        std::optional<InputAssemblyDesc> input_assembly_desc;
        std::optional<RasterizerDesc> rasterizer_desc;
    };

    //TODO: add pipeline properties
    struct ShaderData {
        ShaderReflection reflection;

        std::vector<char> spirv_code;
        std::vector<char> metal_code;
        //each entry point has own code in dxil
        std::unordered_map<std::string, std::vector<char>> dxil_codes;
    };


    template<typename T>
    concept IsResource = requires {
        std::is_same_v<T, DnmGL::Buffer> || std::is_same_v<T, DnmGL::Image> || std::is_same_v<T, DnmGL::Sampler>;
    };

    enum class MessageType {
        eInfo,
        eWarning,
        eUnknown,
        eOutOfMemory,
        eInvalidBehavior,
        eUnsupportedDevice,
        eOutOfDateSwapchain,
        eInvalidState,
        eShaderCompilationFailed,
        eDeviceLost,
        eGraphicsBackendInternal,
    };

    enum class GraphicsBackend {
        eVulkan,
        eD3D12,
    };

    enum class WindowType {
        eNone = 0,
        eWindows,
        eMac,
        eIos,
        eWayland,
        eAndroid,
        eX11,
    };

    struct WinWindowHandle {
        void *hwnd;
        void *hInstance;
    };

    //TODO: other window managers
    struct MacWindowHandle {
        
    };

    struct IosWindowHandle {
        
    };

    struct AndroidWindowHandle {
        
    };

    struct WaylandWindowHandle {
        
    };

    struct X11WindowHandle {
        
    };

    using WindowHandle = std::variant<
                            std::nullopt_t,
                            WinWindowHandle, 
                            MacWindowHandle,
                            IosWindowHandle,
                            WaylandWindowHandle,
                            AndroidWindowHandle,
                            X11WindowHandle>;
                            
    inline WindowType GetWindowType(const WindowHandle& handle) {
        switch (handle.index()) {
            case 1: return WindowType::eWindows;
            case 2: return WindowType::eMac;
            case 3: return WindowType::eIos;
            case 4: return WindowType::eWayland;
            case 5: return WindowType::eAndroid;
            case 6: return WindowType::eX11;
            default: return WindowType::eNone;
        }
    }

    struct SwapchainSettings {
        Uint2 window_extent = {1, 1};
        //eUndefined no depth buffer
        ImageFormat depth_buffer_format = ImageFormat::eD16Norm;
        SampleCount msaa = SampleCount::e1;
        bool Vsync;

        bool operator<=>(const SwapchainSettings &) const = default;
    };

    struct ContextDesc {
        WindowHandle window_handle;
        std::filesystem::path shader_directory;
        SwapchainSettings swapchain_settings;
    };

    using CallbackFunc = std::function<void(std::string_view message, MessageType error, std::string_view source)>;

    //TODO: maybe i do checking api support 
    extern "C" DNMGL_API DnmGL::Context *CreateVulkanContext();
    extern "C" DNMGL_API DnmGL::Context *CreateD3D12Context();
    extern "C" DNMGL_API DnmGL::Context *CreateMetalContext();

    class RHIObject {
    public:
        explicit constexpr RHIObject(Context &context) noexcept
            : context(&context) {}

        RHIObject(RHIObject &&) = delete;
        RHIObject(RHIObject &) = delete;
        RHIObject& operator=(RHIObject &&) = delete;
        RHIObject& operator=(RHIObject &) = delete;

        Context *context;
    };

    class Framebuffer : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::Framebuffer>;
        constexpr Framebuffer(Context& context, const DnmGL::FramebufferDesc& desc) noexcept;
        virtual ~Framebuffer() = default;

        void SetAttachments(
            std::span<const DnmGL::RenderAttachment> color_attachments,
            DnmGL::RenderAttachment depth_stencil_attachment);

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
        [[nodiscard]] constexpr auto HasMsaa() const noexcept { return m_desc.msaa != SampleCount::e1; }
        [[nodiscard]] constexpr auto HasDepthAttachment() const noexcept { return has_depth_attachment; }
        [[nodiscard]] constexpr auto HasStencilAttachment() const noexcept { return has_stencil_attachment; }
        [[nodiscard]] constexpr auto ColorAttachmentCount() const noexcept { return m_desc.color_attachment_formats.size(); }
    protected:
        virtual void ISetAttachments(
            std::span<const DnmGL::RenderAttachment> attachments, 
            DnmGL::RenderAttachment depth_stencil_attachment) = 0;

        constexpr void IsValidColorAttachment(const Image *image, uint32_t i) const noexcept;
        constexpr void IsValidDepthStencilAttachment(const Image *image) const noexcept;

        DnmGL::FramebufferDesc m_desc;
        bool has_depth_attachment : 1{};
        bool has_stencil_attachment : 1{};
    };

    class Buffer : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::Buffer>;
        constexpr Buffer(Context& ctx, const DnmGL::BufferDesc& desc) noexcept;
            
        virtual ~Buffer() = default;

        template <typename T = uint8_t>
        [[nodiscard]] constexpr T *GetMappedPtr() const noexcept;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        uint8_t *m_mapped_ptr;

        DnmGL::BufferDesc m_desc;
    };

    class Image : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::Image>;
        constexpr Image(Context& context, const DnmGL::ImageDesc& desc) noexcept;
                     
        virtual ~Image() = default;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        DnmGL::ImageDesc m_desc;
    };

    class Sampler : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::Sampler>;
        constexpr Sampler(Context& context, const DnmGL::SamplerDesc& desc) noexcept
            : RHIObject(context),
            m_desc(desc) {}
                     
        virtual ~Sampler() = default;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        DnmGL::SamplerDesc m_desc;
    };

    class GraphicsPipeline : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::GraphicsPipeline>;
        constexpr GraphicsPipeline(Context& ctx, const GraphicsPipelineDesc& desc) noexcept;
        
        virtual ~GraphicsPipeline() = default;

        void SetResource(std::string_view resource_name, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count);
        void SetResource(std::string_view resource_name, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count);
        void SetResource(std::string_view resource_name, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count);

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
        [[nodiscard]] constexpr auto HasMsaa() const noexcept { return m_rasterizer_desc.msaa != SampleCount::e1; }
        [[nodiscard]] constexpr auto HasDepthAttachment() const noexcept { return has_depth_attachment; }
        [[nodiscard]] constexpr auto HasStencilAttachment() const noexcept { return has_stencil_attachment; }
        [[nodiscard]] constexpr auto ColorAttachmentCount() const noexcept { return m_rasterizer_desc.color_attachment_formats.size(); }

        [[nodiscard]] constexpr auto &GetShaderReflection() const noexcept { return m_shader_data->reflection; };

        template <typename T>
        requires IsResource<T>
        [[nodiscard]] T *GetPipelineResource(std::string_view name) const noexcept;
    protected:
        void SetPipelineResources();

        virtual void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) = 0;
        virtual void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) = 0;
        virtual void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) = 0;

        const ShaderData *m_shader_data;
        GraphicsPipelineDesc m_desc;
        RasterizerDesc m_rasterizer_desc;
        InputAssemblyDesc m_input_assembly_desc;
        DepthStencilDesc m_depth_stencil_desc;

        std::unordered_map<std::string, 
            std::variant<DnmGL::Buffer::Ptr, DnmGL::Image::Ptr, DnmGL::Sampler::Ptr>> m_pipeline_resources;

        bool has_depth_attachment : 1{};
        bool has_stencil_attachment : 1{};

    private:
        friend CommandBuffer;
    };

    class ComputePipeline : public RHIObject {
    public:
        using Ptr = std::unique_ptr<ComputePipeline>;
        constexpr ComputePipeline(Context& ctx, std::string_view shader_name) noexcept;
        virtual ~ComputePipeline() = default;

        void SetResource(std::string_view resource_name, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count);
        void SetResource(std::string_view resource_name, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count);
        void SetResource(std::string_view resource_name, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count);

        [[nodiscard]] constexpr std::string_view GetShader() const noexcept { return m_shader_name; }

        [[nodiscard]] constexpr auto &GetShaderReflection() const noexcept { return m_shader_data->reflection; };

        template <typename T>
        requires IsResource<T>
        [[nodiscard]] T *GetPipelineResource(std::string_view name) const noexcept;
    protected:
        void SetPipelineResources();

        virtual void ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) = 0;
        virtual void ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) = 0;
        virtual void ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) = 0;
        
        const std::string m_shader_name;
        const ShaderData *m_shader_data;

        std::unordered_map<std::string, 
            std::variant<DnmGL::Buffer::Ptr, DnmGL::Image::Ptr, DnmGL::Sampler::Ptr>> m_pipeline_resources;
    };

    class CommandBuffer : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGL::CommandBuffer>;
        explicit constexpr CommandBuffer(Context& context) noexcept : RHIObject(context) {}

        virtual ~CommandBuffer() = default;

        void Begin();
        void End();

        void BeginRendering(const BeginRenderingDesc& desc);
        void EndRendering();

        void BeginCopyPass();
        void EndCopyPass();

        void BeginComputePass();
        void EndComputePass();

        void Draw(uint32_t vertex_count, uint32_t instance_count);
        void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset);

        void ComputeDispatch(const DnmGL::ComputePipeline *pipeline, std::string_view kernel, uint16_t x = 1, uint16_t y = 1, uint16_t z = 1);

        void SetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth);
        void SetScissor(Uint2 extent, Uint2 offset);

        void CopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc);
        void CopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc);
        void CopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc);
        void CopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc);

        void GenerateMipmaps(DnmGL::Image *image);

        void BindVertexBuffer(const DnmGL::Buffer *buffer, uint64_t offset);
        void BindIndexBuffer(const DnmGL::Buffer *buffer, uint64_t offset, DnmGL::IndexType index_type);

        template <typename T> void UploadData(DnmGL::Buffer *buffer, std::span<const T> data, uint32_t offset);
        //TODO: maybe has UploadImageData struct
        //! is not subresource
        template <typename T> void UploadData(DnmGL::Image *image, 
                                                ImageSubresource subresource,
                                                std::span<const T> data, 
                                                Uint3 copy_extent, 
                                                Uint3 copy_offset);

        [[nodiscard]] constexpr auto GetPassType() const noexcept { return active_pass; }
    protected:
        virtual void IBegin() = 0;
        virtual void IEnd() = 0;

        //! is not subresource
        virtual void IUploadData(DnmGL::Image *image, 
                                ImageSubresource subresource, 
                                const void *data, 
                                Uint3 copy_extent, 
                                Uint3 copy_offset) = 0;
        virtual void IUploadData(DnmGL::Buffer *buffer, const void *data, uint32_t size, uint32_t offset) = 0;

        virtual void IBeginRendering(const BeginRenderingDesc& desc) = 0;
        virtual void IEndRendering() = 0;

        virtual void IBeginCopyPass() = 0;
        virtual void IEndCopyPass() = 0;

        virtual void IBeginComputePass() = 0;
        virtual void IEndComputePass() = 0;

        virtual void IDraw(uint32_t vertex_count, uint32_t instance_count) = 0;
        virtual void IDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) = 0;
        virtual void IComputeDispatch(const DnmGL::ComputePipeline *pipeline, std::string_view kernel, uint16_t x = 1, uint16_t y = 1, uint16_t z = 1) = 0;

        virtual void ISetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) = 0;
        virtual void ISetScissor(Uint2 extent, Uint2 offset) = 0;

        virtual void ICopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) = 0;
        virtual void ICopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) = 0;
        virtual void ICopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) = 0;
        virtual void ICopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) = 0;

        virtual void IGenerateMipmaps(DnmGL::Image *image) = 0;

        virtual void IBindVertexBuffer(const DnmGL::Buffer *buffer, uint64_t offset) = 0;
        virtual void IBindIndexBuffer(const DnmGL::Buffer *buffer, uint64_t offset, DnmGL::IndexType index_type) = 0;

        constexpr void IsValidBeginRenderingDesc(const BeginRenderingDesc& desc) const noexcept;

        ComputePipeline *active_compute_pipeline{};
        GraphicsPipeline *active_graphics_pipeline{};
        Framebuffer *active_framebuffer{};
        CommandBufferPassType active_pass{};
    };

    class DNMGL_API Context {
        void CreateShaderCompiler();
        void DestroyShaderCompiler() const;
    public:
        Context() { CreateShaderCompiler(); }
        virtual ~Context() { DestroyShaderCompiler(); };

        [[nodiscard]] virtual GraphicsBackend GetGraphicsBackend() const noexcept = 0;

        void Init(const ContextDesc &);
        void SetSwapchainSettings(const SwapchainSettings &settings);

        //offline rendering
        virtual void ExecuteCommands(const std::function<bool(CommandBuffer *)>& func) = 0;
        //ExecuteCommands + present image
        virtual void Render(const std::function<bool(CommandBuffer *)>& func) = 0;
        virtual void WaitForGPU() = 0;

        [[nodiscard]] virtual DnmGL::Buffer::Ptr CreateBuffer(const DnmGL::BufferDesc &) noexcept = 0;
        [[nodiscard]] virtual DnmGL::Image::Ptr CreateImage(const DnmGL::ImageDesc &) noexcept = 0;
        [[nodiscard]] virtual DnmGL::Sampler::Ptr CreateSampler(const DnmGL::SamplerDesc &) noexcept = 0;
        [[nodiscard]] virtual DnmGL::Framebuffer::Ptr CreateFramebuffer(const DnmGL::FramebufferDesc &) noexcept = 0;
        [[nodiscard]] virtual DnmGL::GraphicsPipeline::Ptr CreateGraphicsPipeline(const DnmGL::GraphicsPipelineDesc &) noexcept = 0;
        [[nodiscard]] virtual DnmGL::ComputePipeline::Ptr CreateComputePipeline(std::string_view) noexcept = 0;
        [[nodiscard]] virtual ContextState GetContextState() noexcept = 0;
        
        //compile shader and add cache if shader exists override shader data in cache
        [[nodiscard]] std::optional<std::string> CompileShader(std::string_view shader_name) const noexcept;
        //write shader data to dnmShader file if not exists get shader data from shader and write, and add cache
        [[nodiscard]] std::optional<std::string> WriteShaderData(std::string_view shader_name) const noexcept;
        //if dnmShader file exists in shader folder read if not exists compile slang file, and add cache
        [[nodiscard]] std::optional<std::string> ReadOrCompileShader(std::string_view shader_name) const noexcept;

        //TODO: this func must be private
        //cache -> .dnmShader -> .slang
        [[nodiscard]] std::expected<const ShaderData *, std::string> GetShaderData(std::string_view shader_name) const noexcept;

        [[nodiscard]] constexpr DnmGL::Image *GetPlaceholderImage() const noexcept { return placeholder_image; };
        [[nodiscard]] constexpr DnmGL::Sampler *GetPlaceholderSampler() const noexcept { return placeholder_sampler; };
        [[nodiscard]] constexpr const std::filesystem::path& GetShaderDirectory() const noexcept { return shader_directory; };
        [[nodiscard]] constexpr const auto& GetSwapchainSettings() const noexcept { return swapchain_settings; };
        [[nodiscard]] constexpr std::filesystem::path GetShaderPath(std::string_view filename) const noexcept;
        constexpr void SetCallbackFunc(CallbackFunc func) noexcept { callback_func.swap(func); };
        constexpr void Message(
            const std::string_view message,
            const MessageType severity,
            const std::string_view source = std::source_location::current().function_name()) const {
            if (callback_func) callback_func(message, severity, source);
        };
    protected:
        virtual void IInit(const ContextDesc &) = 0;
        virtual void ISetSwapchainSettings(const SwapchainSettings &settings) = 0;

        DnmGL::Image *placeholder_image{};
        DnmGL::Sampler *placeholder_sampler{};
        ShaderCompiler *shader_compiler{};
        CallbackFunc callback_func{};
        std::filesystem::path shader_directory{};
        SwapchainSettings swapchain_settings{};
    };

    //TODO: fill this
    constexpr void IsValidImageSubresource(const DnmGL::Image &image, const ImageSubresource subresource) noexcept {
        DnmGLAssert(image.GetDesc().mipmap_levels >= subresource.base_mipmap + subresource.mipmap_level,
        "Mipmap range exceeds image mip levels");

        if (image.GetDesc().type == ImageType::e2D) {
            DnmGLAssert(image.GetDesc().extent.z >= subresource.layer_count + subresource.base_layer,
                "Layer range exceeds image depth");
        }

        if (subresource.type == ImageSubresourceType::e1D) {
            DnmGLAssert(image.GetDesc().type == ImageType::e1D,
                "Subresource is 1D but image is not 1D");
        }
        else if (subresource.type == ImageSubresourceType::e2D) {
            DnmGLAssert(image.GetDesc().type == ImageType::e2D,
                "Subresource is 2D but image is not 2D");
        }
        else if (subresource.type == ImageSubresourceType::e2DArray) {
            DnmGLAssert(image.GetDesc().type == ImageType::e2D,
                "2D array subresource requires 2D image");
        }
        else if (subresource.type == ImageSubresourceType::e3D) {
            DnmGLAssert(image.GetDesc().type == ImageType::e3D,
                "Subresource is 3D but image is not 3D");
        }
        else if (subresource.type == ImageSubresourceType::eCube) {
            DnmGLAssert(image.GetDesc().type == ImageType::e2D,
                "Cube subresource requires 2D image");
            DnmGLAssert(subresource.layer_count == 6,
                "Cube subresource must have 6 layers");
        }
    };

    inline void CommandBuffer::Begin() {
        IBegin();
    }

    inline void CommandBuffer::End() {
        if (active_pass != CommandBufferPassType::eNone) {
            switch (active_pass) {
                case CommandBufferPassType::eNone: std::unreachable();
                case CommandBufferPassType::eTransfer: EndCopyPass(); break;
                case CommandBufferPassType::eCompute: EndComputePass(); break;
                case CommandBufferPassType::eRendering: EndRendering(); break;
            }
        }

        IEnd();
    }
    
    inline void CommandBuffer::BeginRendering(const BeginRenderingDesc& desc) {
        DnmGLAssert(active_pass == CommandBufferPassType::eNone, 
        "BeginRendering cannot be call in some pass")

        IsValidBeginRenderingDesc(desc);

        active_graphics_pipeline = desc.pipeline;
        active_framebuffer = desc.framebuffer;
        IBeginRendering(desc);
        active_pass = CommandBufferPassType::eRendering;
    }

    inline void CommandBuffer::EndRendering() {
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, 
        "BeginRendering must be called before EndRendering")

        active_graphics_pipeline = nullptr;
        active_framebuffer = nullptr;
        IEndRendering();
        active_pass = CommandBufferPassType::eNone;
    }

    inline void CommandBuffer::BeginComputePass() {
        DnmGLAssert(active_pass == CommandBufferPassType::eNone, 
        "BeginCompute cannot be call in some pass")

        active_compute_pipeline = nullptr;
        IBeginComputePass();
        active_pass = CommandBufferPassType::eCompute;
    }

    inline void CommandBuffer::EndComputePass() {
        DnmGLAssert(active_pass == CommandBufferPassType::eCompute, 
        "BeginCompute must be called before EndCompute")

        active_compute_pipeline = nullptr;
        IEndComputePass();
        active_pass = CommandBufferPassType::eNone;
    }

    inline void CommandBuffer::BeginCopyPass() {
        DnmGLAssert(active_pass == CommandBufferPassType::eNone, 
        "BeginCopyPass cannot be call in some pass")

        IBeginCopyPass();
        active_pass = CommandBufferPassType::eTransfer;
    }

    inline void CommandBuffer::EndCopyPass() {
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, 
        "BeginCopyPass must be called before EndCopyPass")

        IEndCopyPass();
        active_pass = CommandBufferPassType::eNone;
    }

    inline void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count) {
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")

        if (vertex_count || instance_count)
            IDraw(vertex_count, instance_count);
    }

    inline void CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) {
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")

        if (index_count || instance_count)
            IDrawIndexed(index_count, instance_count, vertex_offset);
    }

    inline void CommandBuffer::SetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")

        ISetViewport(extent, offset, min_depth, max_depth);
    }

    inline void CommandBuffer::SetScissor(Uint2 extent, Uint2 offset) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")

        ISetScissor(extent, offset);
    }

    inline void CommandBuffer::CopyImageToBuffer(const DnmGL::ImageToBufferCopyDesc& desc) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(desc.src_image, "src_image cannot be null")
        DnmGLAssert(desc.dst_buffer, "dst_buffer cannot be null")

        ICopyImageToBuffer(desc);
    }

    inline void CommandBuffer::CopyImageToImage(const DnmGL::ImageToImageCopyDesc& desc) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(desc.src_image, "src_image cannot be null")
        DnmGLAssert(desc.dst_image, "dst_image cannot be null")

        ICopyImageToImage(desc);
    }

    inline void CommandBuffer::CopyBufferToImage(const DnmGL::BufferToImageCopyDesc& desc) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(desc.dst_image, "dst_image cannot be null")
        DnmGLAssert(desc.src_buffer, "src_buffer cannot be null")

        ICopyBufferToImage(desc);
    }

    inline void CommandBuffer::CopyBufferToBuffer(const DnmGL::BufferToBufferCopyDesc& desc) {
        //TODO: check bounds
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(desc.src_buffer, "src_buffer cannot be null")
        DnmGLAssert(desc.dst_buffer, "dst_buffer cannot be null")

        ICopyBufferToBuffer(desc);
    }

    inline void CommandBuffer::GenerateMipmaps(DnmGL::Image *image) {
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(image, "image cannot be null")
        if (image->GetDesc().mipmap_levels == 1) return;

        IGenerateMipmaps(image);
    }

    inline void CommandBuffer::BindVertexBuffer(const DnmGL::Buffer *buffer, uint64_t offset) {
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")
        DnmGLAssert(buffer, "buffer cannot be null")
        
        IBindVertexBuffer(buffer, offset);
    }

    inline void CommandBuffer::BindIndexBuffer(const DnmGL::Buffer *buffer, uint64_t offset, DnmGL::IndexType index_type) {
        DnmGLAssert(active_pass == CommandBufferPassType::eRendering, "this function must be call in rendering pass")
        DnmGLAssert(buffer, "buffer cannot be null")

        IBindIndexBuffer(buffer, offset, index_type);
    }
    
    template <typename T> 
    void CommandBuffer::UploadData(DnmGL::Buffer *buffer, std::span<const T> data, uint32_t offset) {
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(buffer, "buffer cannot be null")
        if (data.empty()) return;

        IUploadData(buffer, data.data(), data.size()  *sizeof(T), offset);
    }

    template <typename T>
    void CommandBuffer::UploadData(DnmGL::Image *image,
                                            const ImageSubresource subresource,
                                            std::span<const T> data,
                                            const Uint3 copy_extent,
                                            const Uint3 copy_offset) {
        DnmGLAssert(active_pass == CommandBufferPassType::eTransfer, "this function must be call in transfer pass")
        DnmGLAssert(image, "image cannot be null")
        if (data.empty()) return;

        const auto copy_size = copy_extent.x  *copy_extent.y  *copy_extent.z  *GetFormatSize(image->GetDesc().format);
        if (data.size() * sizeof(T) < copy_size) [[unlikely]]
            context->Message(
                std::format("data size must be equal or bigger than copy_extent pixel count; data.size(): {}, copy_extent pixel count {}",
                data.size()  *sizeof(T), copy_size), 
                MessageType::eInvalidBehavior);

        IUploadData(image, subresource, data.data(), copy_extent, copy_offset);
    }

    constexpr Image::Image(Context& context, const DnmGL::ImageDesc& desc) noexcept
    : RHIObject(context), m_desc(desc) {
        DnmGLAssert(m_desc.mipmap_levels != 0, "mipmap level cannot be 0")
        DnmGLAssert(m_desc.extent.x != 0 || m_desc.extent.y != 0 || m_desc.extent.z != 0, "extent values cannot be 0")
        if (m_desc.type == ImageType::e1D) {
            DnmGLAssert((m_desc.extent.y == 1 && m_desc.extent.z == 1), 
                                "type if ImageType::e1D, x and z extent is must be 1 (1D arrays not supported)")
        }
    }

    constexpr std::filesystem::path Context::GetShaderPath(const std::string_view filename) const noexcept {
        return shader_directory / filename += ".slang";
    };

    constexpr Framebuffer::Framebuffer(Context& context, const DnmGL::FramebufferDesc &desc) noexcept
        : RHIObject(context), m_desc(desc) {
        if (m_desc.depth_stencil_format == ImageFormat::eUndefined) {
            DnmGLAssert(!m_desc.create_depth_buffer,
                        "if m_desc.create_depth_buffer is true m_desc.depth_stencil_format must be DnmGL::ImageFormat::eD16Norm or DnmGL::ImageFormat::eD32Float")
            return;
        }
        if (IsDepthFormat(m_desc.depth_stencil_format)) {
            has_depth_attachment = true;
            return;
        }
        if (IsDepthStencilFormat(m_desc.depth_stencil_format)) {
            has_depth_attachment = true;
            has_stencil_attachment = true;

            DnmGLAssert(!m_desc.create_depth_buffer,
                        "if m_desc.create_depth_buffer is true m_desc.depth_stencil_format must be DnmGL::ImageFormat::eD16Norm or DnmGL::ImageFormat::eD32Float")
            return;
        }
        DnmGLAssert(false,
                    "m_desc.depth_stencil_format must be depth or depth stencil format")
    }

    inline void Framebuffer::SetAttachments(
        const std::span<const DnmGL::RenderAttachment> color_attachments,
        const DnmGL::RenderAttachment depth_stencil_attachment) {
        //TODO: write better message
        DnmGLAssert(m_desc.color_attachment_formats.size() == color_attachments.size(), "")

        for (const auto i : Counter(color_attachments.size())) {
            IsValidColorAttachment(color_attachments[i].image, i);
        }

        if (depth_stencil_attachment.image != nullptr || HasStencilAttachment()) {
            IsValidDepthStencilAttachment(depth_stencil_attachment.image);
        }

        ISetAttachments(color_attachments, depth_stencil_attachment);
    }

    constexpr void Framebuffer::IsValidColorAttachment(const Image *image, uint32_t i) const noexcept {
        DnmGLAssert(image, "color attachment cannot be null")

        DnmGLAssert(context == image->context, "framebuffer and image must be created from the same context")

        DnmGLAssert(Uint2{image->GetDesc().extent.x, image->GetDesc().extent.y} == m_desc.extent,
                    "image extent and framebuffer extent must be same")

        DnmGLAssert(image->GetDesc().format == m_desc.color_attachment_formats[i],
                    "image format must be same with framebuffer format")

        DnmGLAssert(image->GetDesc().usage_flags.Has(ImageUsageBits::eColorAttachment),
                    "image usage_flags has ImageUsageBits::eColorAttachment")
    }

    constexpr void Framebuffer::IsValidDepthStencilAttachment(const Image *image) const noexcept {
        DnmGLAssert(image, "the depth-stencil attachment cannot be nullptr if the framebuffer has a stencil attachment")

        DnmGLAssert(context == image->context, "framebuffer and image must be created from the same context")

        DnmGLAssert(Uint2{image->GetDesc().extent.x, image->GetDesc().extent.y} == m_desc.extent,
                    "image extent and framebuffer extent must be same")

        DnmGLAssert(image->GetDesc().format == m_desc.depth_stencil_format,
                    "image format must be same with framebuffer format")

        DnmGLAssert(image->GetDesc().sample_count == m_desc.msaa,
                    "image sample_count must be same with framebuffer msaa value")

        DnmGLAssert(image->GetDesc().usage_flags.Has(ImageUsageBits::eDepthStencilAttachment),
                    "image usage_flags has ImageUsageBits::eDepthStencilAttachment")
    }

    constexpr Buffer::Buffer(Context& ctx, const DnmGL::BufferDesc& desc) noexcept
        : RHIObject(ctx), m_mapped_ptr(nullptr), m_desc(desc) {
        if (m_desc.usage_flags.Has(BufferUsageBits::eUniform)) {
            DnmGLAssert(desc.element_size % 256, "uniform buffers must be 256 align");
        }
    }

    template <typename T>
    constexpr T *Buffer::GetMappedPtr() const noexcept {
        DnmGLAssert(m_desc.memory_host_access != MemoryHostAccess::eNone,
                    "MemoryHostAccess::eNone, must be MemoryHostAccess::eWrite or MemoryHostAccess::eReadWrite");

        return reinterpret_cast<T *>(m_mapped_ptr);
    }

    constexpr GraphicsPipeline::GraphicsPipeline(Context& ctx, const GraphicsPipelineDesc& desc) noexcept
        : RHIObject(ctx), m_desc(desc) {
        const auto shader_data = ctx.GetShaderData(desc.shader_name);
        if (!shader_data.has_value()) {
            ctx.Message(shader_data.error(),
                       MessageType::eShaderCompilationFailed);
        }

        m_shader_data = shader_data.value();
        DnmGLAssert(m_shader_data->reflection.shader_type == ShaderType::eGraphicsShader, "shader must be eGraphicsShader");

        m_input_assembly_desc = *m_desc.input_assembly_desc;
        m_rasterizer_desc = *m_desc.rasterizer_desc;
        m_depth_stencil_desc = *m_desc.depth_stencil_desc;

        m_desc.rasterizer_desc = &m_rasterizer_desc;
        m_desc.depth_stencil_desc = &m_depth_stencil_desc;
        m_desc.input_assembly_desc = &m_input_assembly_desc;

        has_depth_attachment = IsDepthFormat(m_depth_stencil_desc.depth_stencil_format);
        has_stencil_attachment = IsDepthStencilFormat(m_depth_stencil_desc.depth_stencil_format);

        for (const auto &res : m_shader_data->reflection.resources | std::ranges::views::values) {
            if (res.create_desc.index() == 0)
                continue;

            if (const auto *buffer_desc = std::get_if<DnmGL::BufferDesc>(&res.create_desc);
                buffer_desc)
                m_pipeline_resources.emplace(res.name, context->CreateBuffer(*buffer_desc));
            else if (const auto *image_desc = std::get_if<DnmGL::ImageDesc>(&res.create_desc);
                image_desc)
                m_pipeline_resources.emplace(res.name, context->CreateImage(*image_desc));
            else if (const auto *sampler_desc = std::get_if<DnmGL::SamplerDesc>(&res.create_desc);
                sampler_desc)
                m_pipeline_resources.emplace(res.name, context->CreateSampler(*sampler_desc));
        }

        //TODO: check somethings
    }

    inline void GraphicsPipeline::SetResource(const std::string_view resource_name, const BufferResourceDesc &buffer_desc, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(buffer_desc.buffer, "buffer cannot be nullptr");
        DnmGLAssert(context == buffer_desc.buffer->context, "buffer and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:"
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type == ResourceType::eReadonlyBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eReadonlyResource)) {
                context->Message("buffer must be have BufferUsageBits::eReadonlyResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eWritableBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eWritableResource)) {
                context->Message("buffer must be have BufferUsageBits::eWritableResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eUniformBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eUniform)) {
                context->Message("buffer must be have BufferUsageBits::eUniform, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else {
            context->Message("resource type is not buffer, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (buffer_desc.buffer->GetDesc().element_count < buffer_desc.first_element + buffer_desc.element_count) {
            context->Message("first_element + element_count less than element count, resource name:"
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        ISetResource(resource, buffer_desc, resource_index, resource_count);
    }

    inline void GraphicsPipeline::SetResource(const std::string_view resource_name, const ImageResourceDesc &image_desc, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(image_desc.image, "image cannot be nullptr");
        DnmGLAssert(context == image_desc.image->context, "image and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:"
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type == ResourceType::eReadonlyImage) {
            if (!image_desc.image->GetDesc().usage_flags.Has(ImageUsageBits::eReadonlyResource)) {
                context->Message("image must be have imageUsageBits::eReadonlyResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eWritableImage) {
            if (!image_desc.image->GetDesc().usage_flags.Has(ImageUsageBits::eWritableResource)) {
                context->Message("image must be have imageUsageBits::eWritableResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else {
            context->Message("resource type is not image, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        IsValidImageSubresource(*image_desc.image, image_desc.subresource);

        ISetResource(resource, image_desc, resource_index, resource_count);
    }

    inline void GraphicsPipeline::SetResource(const std::string_view resource_name, const DnmGL::Sampler *sampler, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(sampler, "sampler cannot be nullptr");
        DnmGLAssert(context == sampler->context, "sampler and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:"
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type != ResourceType::eSampler) {
            context->Message("resource type is not image, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        ISetResource(resource, sampler, resource_index, resource_count);
    }

    template <typename T>
    requires IsResource<T>
    [[nodiscard]] inline T *GraphicsPipeline::GetPipelineResource(std::string_view name) const noexcept { 
        const auto it = m_pipeline_resources.find(std::string(name));
        return it == m_pipeline_resources.end() ? std::get_if<typename T::Ptr>(&it->second)->get() : nullptr;
    }

    inline void GraphicsPipeline::SetPipelineResources() {
        for (const auto &[name, resource] : m_pipeline_resources) {
            if (const auto *buffer = std::get_if<DnmGL::Buffer::Ptr>(&resource);
                buffer) {
                SetResource(name, {
                            buffer->get(),
                            0,
                            buffer->get()->GetDesc().element_count
                            }, 0, -1);
            }
            else if (const auto *image = std::get_if<DnmGL::Image::Ptr>(&resource);
                image) {
                SetResource(name, {
                            image->get(),
                            {}
                            }, 0, -1);
            }
            else if (const auto *sampler = std::get_if<DnmGL::Sampler::Ptr>(&resource);
                sampler) {
                SetResource(name, sampler->get(),
                            0, -1);
            }
        }
    }

    constexpr ComputePipeline::ComputePipeline(Context& ctx, const std::string_view shader_name) noexcept
        : RHIObject(ctx), m_shader_name(shader_name) {
        const auto shader_data = ctx.GetShaderData(shader_name);
        if (!shader_data.has_value()) ctx.Message(shader_data.error(),
                                                  MessageType::eShaderCompilationFailed);

        m_shader_data = shader_data.value();
        DnmGLAssert(m_shader_data->reflection.shader_type == ShaderType::eComputeShader, "shader must be eComputeShader");

        for (const auto &res : m_shader_data->reflection.resources | std::ranges::views::values) {
            if (res.create_desc.index() == 0)
                continue;

            if (const auto *buffer_desc = std::get_if<DnmGL::BufferDesc>(&res.create_desc);
                buffer_desc)
                m_pipeline_resources.emplace(res.name, context->CreateBuffer(*buffer_desc));
            else if (const auto *image_desc = std::get_if<DnmGL::ImageDesc>(&res.create_desc);
                image_desc)
                m_pipeline_resources.emplace(res.name, context->CreateImage(*image_desc));
            else if (const auto *sampler_desc = std::get_if<DnmGL::SamplerDesc>(&res.create_desc);
                sampler_desc)
                m_pipeline_resources.emplace(res.name, context->CreateSampler(*sampler_desc));
        }
    }

    inline void ComputePipeline::SetResource(const std::string_view resource_name, const BufferResourceDesc &buffer_desc, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(buffer_desc.buffer, "buffer cannot be nullptr");
        DnmGLAssert(context == buffer_desc.buffer->context, "buffer and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:" 
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type == ResourceType::eReadonlyBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eReadonlyResource)) {
                context->Message("buffer must be have BufferUsageBits::eReadonlyResource, resource name:" 
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eWritableBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eWritableResource)) {
                context->Message("buffer must be have BufferUsageBits::eWritableResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eUniformBuffer) {
            if (!buffer_desc.buffer->GetDesc().usage_flags.Has(BufferUsageBits::eUniform)) {
                context->Message("buffer must be have BufferUsageBits::eUniform, resource name:" 
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else {
            context->Message("resource type is not buffer, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (buffer_desc.buffer->GetDesc().element_count < buffer_desc.first_element + buffer_desc.element_count) {
            context->Message("first_element + element_count less than element count, resource name:"
                             + std::string(resource_name), MessageType::eWarning);
            return;            
        }

        ISetResource(resource, buffer_desc, resource_index, resource_count);
    }

    inline void ComputePipeline::SetResource(const std::string_view resource_name, const ImageResourceDesc &image_desc, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(image_desc.image, "image cannot be nullptr");
        DnmGLAssert(context == image_desc.image->context, "image and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:" 
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type == ResourceType::eReadonlyImage) {
            if (!image_desc.image->GetDesc().usage_flags.Has(ImageUsageBits::eReadonlyResource)) {
                context->Message("image must be have imageUsageBits::eReadonlyResource, resource name:" 
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else if (resource.type == ResourceType::eWritableImage) {
            if (!image_desc.image->GetDesc().usage_flags.Has(ImageUsageBits::eWritableResource)) {
                context->Message("image must be have imageUsageBits::eWritableResource, resource name:"
                                 + std::string(resource_name), MessageType::eWarning);
                return;
            }
        }
        else {
            context->Message("resource type is not image, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        IsValidImageSubresource(*image_desc.image, image_desc.subresource);

        ISetResource(resource, image_desc, resource_index, resource_count);
    }

    inline void ComputePipeline::SetResource(const std::string_view resource_name, const DnmGL::Sampler *sampler, const uint16_t resource_index, uint16_t resource_count) {
        DnmGLAssert(sampler, "sampler cannot be nullptr");
        DnmGLAssert(context == sampler->context, "sampler and pipeline must use the same context");

        const auto it = m_shader_data->reflection.resources.find(std::string(resource_name));
        if (it == m_shader_data->reflection.resources.end()) {
            context->Message("resource not found, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        const auto &resource = it->second;

        if (resource_index >= resource.resource_count) {
            context->Message("array index must be less than resource count, resource name:" 
                             + std::string(resource_name), MessageType::eWarning);
            return;
        }

        if (resource_count > resource.resource_count - resource_index) {
            resource_count = resource.resource_count - resource_index;
        }

        if (resource.type != ResourceType::eSampler) {
            context->Message("resource type is not image, resource name:" + std::string(resource_name), MessageType::eWarning);
            return;
        }

        ISetResource(resource, sampler, resource_index, resource_count);
    }

    template <typename T>
        requires IsResource<T>
    [[nodiscard]] inline T *ComputePipeline::GetPipelineResource(std::string_view name) const noexcept {
        const auto it = m_pipeline_resources.find(std::string(name));
        return it == m_pipeline_resources.end() ? std::get_if<typename T::Ptr>(&it->second)->get() : nullptr;
    }

    inline void ComputePipeline::SetPipelineResources() {
        for (const auto &[name, resource] : m_pipeline_resources) {
            if (const auto *buffer = std::get_if<DnmGL::Buffer::Ptr>(&resource);
                buffer) {
                SetResource(name, {
                            buffer->get(),
                            0,
                            buffer->get()->GetDesc().element_count
                            }, 0, -1);
            }
            else if (const auto *image = std::get_if<DnmGL::Image::Ptr>(&resource);
                image) {
                SetResource(name, {
                            image->get(),
                            {}
                            }, 0, -1);
            }
            else if (const auto *sampler = std::get_if<DnmGL::Sampler::Ptr>(&resource);
                sampler) {
                SetResource(name, sampler->get(),
                            0, -1);
            }
        }
    }

    inline void CommandBuffer::ComputeDispatch(const DnmGL::ComputePipeline *pipeline, const std::string_view kernel, uint16_t x, uint16_t y, uint16_t z) {
        DnmGLAssert(pipeline, "pipeline cannot be nullptr")

        //TODO: check for kernel

        IComputeDispatch(pipeline, kernel, x, y, z);
    }

    constexpr void CommandBuffer::IsValidBeginRenderingDesc(const BeginRenderingDesc& desc) const noexcept {
        DnmGLAssert(desc.pipeline, "pipeline cannot be null")

        DnmGLAssert(context == desc.pipeline->context, "pipeline and commandBuffer must be created from the same context")
        if (desc.framebuffer) DnmGLAssert(context == desc.framebuffer->context,
                                          "framebuffer and commandBuffer must be created from the same context")

        if (desc.framebuffer) DnmGLAssert(desc.pipeline->ColorAttachmentCount() == desc.framebuffer->ColorAttachmentCount(), "pipeline and framebuffer must be same color attachment count")
        else DnmGLAssert(desc.pipeline->ColorAttachmentCount() == 1, "if using default framebuffer, pipeline color attachment count must be 1")

        if (desc.framebuffer)
            for (const auto i : Counter(desc.pipeline->ColorAttachmentCount())) {
                DnmGLAssert(desc.pipeline->GetDesc().rasterizer_desc->color_attachment_formats[i] == desc.framebuffer->GetDesc().color_attachment_formats[i],
                            "framebuffer and pipeline formats must be same")
            }
        else DnmGLAssert(desc.pipeline->GetDesc().rasterizer_desc->color_attachment_formats[0] == ImageFormat::eRGBA8Norm,
                         "if using default framebuffer, color attachment format ImageFormat::eRGBA8Norm")

        if (desc.framebuffer) DnmGLAssert(desc.pipeline->GetDesc().depth_stencil_desc->depth_stencil_format == desc.framebuffer->GetDesc().depth_stencil_format,
                                          "framebuffer and pipeline formats must be same")
        else DnmGLAssert(desc.pipeline->GetDesc().depth_stencil_desc->depth_stencil_format == context->GetSwapchainSettings().depth_buffer_format,
                         "if using default framebuffer, depth stencil attachment format must same with GetSwapchainSettings().depth_buffer_format")
    }

    inline void Context::Init(const ContextDesc &desc) {
        //TODO: make for other os'es
        if constexpr (_os == OS::eWin) {
            //TODO: write error massage
            DnmGLAssert(GetWindowType(desc.window_handle) == WindowType::eWindows,
                        "");
            const auto [hwnd, hInstance] = std::get<WinWindowHandle>(desc.window_handle);
            DnmGLAssert(hInstance, "WinWindowHandle::hInstance cannot nullptr");
            DnmGLAssert(hwnd, "WinWindowHandle::hwnd cannot nullptr");
        }

        if (desc.swapchain_settings.depth_buffer_format != ImageFormat::eUndefined)
            DnmGLAssert(IsDepthFormat(desc.swapchain_settings.depth_buffer_format),
                    "depth buffer format must be ImageFormat::eD16Norm, ImageFormat::eD32Float");
        DnmGLAssert(desc.swapchain_settings.window_extent.x && desc.swapchain_settings.window_extent.y,
                    "extent values must be bigger than zero")

        IInit(desc);
    }

    inline void Context::SetSwapchainSettings(const SwapchainSettings &settings) {
        DnmGLAssert(IsDepthFormat(settings.depth_buffer_format),
                    "depth buffer format must be ImageFormat::eD16Norm or ImageFormat::eD32Float");
        DnmGLAssert(settings.window_extent.x && settings.window_extent.y,
                    "extent values must be bigger than zero")

        ISetSwapchainSettings(settings);
    }
}

template <>
struct FlagTraits<DnmGL::BufferUsageBits> {
    static constexpr bool is_bitmask = true;
};

template <>
struct FlagTraits<DnmGL::ImageUsageBits> {
    static constexpr bool is_bitmask = true;
};

template <>
struct FlagTraits<DnmGL::ShaderStageBits> {
    static constexpr bool is_bitmask = true;
};