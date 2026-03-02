#define ReadonlyBuffer \
    StructuredBuffer

#define WritableBuffer \
    RWStructuredBuffer

#define UniformBuffer \
    cbuffer
    
#define ReadonlyImage \
    Texture2D

#define Sampler \
    SamplerState

#define WritableImage \
    RWTexture2D

#define ShaderInOut(var, index)  [[vk_location(index)]] var : _##index

#define VertexIndex(var)  var : SV_VertexID
#define InstanceIndex(var)  var : SV_InstanceID
#define VertexPosition(var)  var : SV_Position

enum MemoryType {
    eAuto,
    eHostMemory,
    eDeviceMemory,
};

enum MemoryHostAccess {
    eNone,
    eReadWrite,
    eWrite
};

enum BufferUsageBits {
    eUniform = 0x1,
    eWritableResource = 0x2,
    eReadonlyResource = 0x4,
    eVertex = 0x8,
    eIndex = 0x10,
    eIndirect = 0x20 // not implemented
};

enum ImageFormat {
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
    eD16Norm,
    eD32Float,
    eD24NormS8UInt,
    eD32NormS8UInt,
};

enum VertexFormat {
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

enum ImageUsageBits {
    eReadonlyResource = 0x1,
    eWritableResource = 0x2,
    eColorAttachment = 0x4,
    eDepthStencilAttachment = 0x8,
    eTransientAttachment = 0x10
};

enum SampleCount {
    e1 = 1 << 0,
    e2 = 1 << 1,
    e4 = 1 << 2,
    e8 = 1 << 3,
};

enum ImageType {
    e1D,
    e2D,
    e3D,
};

enum SamplerMipmapMode {
    eNearest,
    eLinear,
};

enum CompareOp {
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

enum SamplerFilter {
    eNearest,
    eLinear,

    eAnisotropyX2 = 2,
    eAnisotropyX4 = 4,
    eAnisotropyX8 = 8,
    eAnisotropyX16 = 16,
};

enum SamplerAddressMode {
    eRepeat,
    eMirroredRepeat,
    eClampToEdge,
    eClampToBorder
};

enum PrimitiveTopology {
    ePointList,
    eLineList,
    eLineStrip,
    eTriangleList,
    eTriangleStrip,
    eTriangleFan,
};

enum PolygonMode {
    eFill,
    eLine,
};

// same with vulkan
enum CullMode {
    eNone,
    eFront,
    eBack,
};

// same with vulkan
enum FrontFace {
    eCounterClockwise,
    eClockwise
};

struct VertexBinding {
    VertexFormat vertex_format;
    uint32_t offset;
};

//TODO: make simpler versions
[__AttributeUsage(_AttributeTargets.Var)]
struct BufferDesc {
    MemoryType memory_type;
    MemoryHostAccess memory_host_access;
    BufferUsageBits buffer_usage;
    uint element_count;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct ImageDesc {
    // if type 2D and z > 1, z is array count
    uint x;
    uint y;
    uint z;
    ImageFormat format;
    ImageUsageBits usage_flags;
    ImageType type;
    uint32_t mipmap_levels = 1;
    SampleCount sample_count = SampleCount::e1;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct SamplerDesc {
    SamplerMipmapMode mipmap_mode;
    CompareOp compare_op;
    SamplerFilter filter;
    SamplerAddressMode address_mode_u = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_v = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_w = SamplerAddressMode::eRepeat;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct InputAssemblyDesc {
    VertexBinding vertex_bindings[8];
    PrimitiveTopology topology;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct RasterizerDesc {
    ImageFormat color_attachment_formats[8];
    PolygonMode polygone_mode;
    CullMode cull_mode;
    FrontFace front_face;
    SampleCount msaa = SampleCount::e1;
    bool color_blend;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct DepthStencilDesc {
    ImageFormat depth_stencil_format;
    CompareOp depth_test_compare_op;
    bool depth_test;
    bool depth_write;
    bool stencil_test;
};
