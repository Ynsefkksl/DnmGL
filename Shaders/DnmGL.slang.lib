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
