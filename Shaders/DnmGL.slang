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

enum MemoryType : uint8_t {
    eAuto,
    eHostMemory,
    eDeviceMemory,
};

enum MemoryHostAccess : uint8_t {
    eNone,
    eReadWrite,
    eWrite
};

enum BufferUsageBits : uint8_t {
    eUniform = 0x1,
    eWritebleResource = 0x2,
    eReadonlyResource = 0x4,
    eVertex = 0x8,
    eIndex = 0x10,
    eIndirect = 0x20 // not implamented
};

[__AttributeUsage(_AttributeTargets.Var)]
struct BufferDesc {
    MemoryType memory_type;
    MemoryHostAccess memory_host_access;
    BufferUsageBits buffer_usage;
    uint element_count;
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

enum SamplerAddressMode : uint8_t {
    eRepeat,
    eMirroredRepeat,
    eClampToEdge,
    eClampToBorder
};

[__AttributeUsage(_AttributeTargets.Struct)]
struct SamplerDesc {
    SamplerMipmapMode mipmap_mode;
    CompareOp compare_op;
    SamplerFilter filter;
    SamplerAddressMode address_mode_u = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_v = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_w = SamplerAddressMode::eRepeat;
};