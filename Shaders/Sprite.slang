#include "DnmGL.slang.lib"

static constexpr const float2 pos[] = {
    float2(-1.0, 1.0),
    float2(-1.0, -1.0),
    float2(1.0, 1.0),
    float2(1.0, -1.0)
};

static constexpr const uint2 uv[] = {
    uint2(0, 0),
    uint2(0, 1),
    uint2(1, 0),
    uint2(1, 1)
};

struct VertexIn {
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};

struct VertexOut {
    float4 vert_pos : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation float4 color : TEXCOORD1;
    nointerpolation float color_factor : TEXCOORD2;
};

struct Sprite {
    float4 color;
    float4 sprite_coords;
    float2 pos;
    float2 scale;
    float angle;
    float color_factor;
};

struct PushConstantStruct {
    float4x4 proj_mtx;
    float4 camera_pos;
};

ReadonlyBuffer<Sprite> sprite_buffer : register(t0);

UniformBuffer pc {
    float4x4 proj_mtx;
    float4 camera_pos;
}

constexpr float2 FastPosCalc(float2 vert_pos, float2 pos, float2 scale, float angle) {
    const float2 rotated_pos = float2(
        vert_pos.x * cos(angle) - vert_pos.y * sin(angle),
        vert_pos.x * sin(angle) + vert_pos.y * cos(angle)
    );
    return (rotated_pos * scale) + pos;
}

[[shader("vertex")]]
VertexOut VertMain(VertexIn in) {
    const var sprite_data = sprite_buffer[in.instance_id];

    const var vert_pos = FastPosCalc(pos[in.vertex_id], sprite_data.pos, sprite_data.scale, sprite_data.angle);

    var out : VertexOut;
    out.uv = float2(
        sprite_data.sprite_coords[uv[in.vertex_id].x * 2u],
        sprite_data.sprite_coords[uv[in.vertex_id].y * 2u + 1u]
        );

    out.color_factor = sprite_data.color_factor;
    out.color = sprite_data.color;
    out.vert_pos = mul(pc.proj_mtx, float4(vert_pos, 0.5, 1));
    return out;
}

/*
    sprite_coords.xy = bottom left
    sprite_coords.zw = up right

    if (vertex_id == 0) {
        uv = (sprite_coords.x, sprite_coords.w);
    }
    if (vertex_id == 1) {
        uv = (sprite_coords.x, sprite_coords.y);
    }
    if (vertex_id == 2) {
        uv = (sprite_coords.z, sprite_coords.w);
    }
    if (vertex_id == 3) {
        uv = (sprite_coords.z, sprite_coords.y);
    }

    uv perfectly matched
    x = 0, z = 1 for uv.x value
    y = 0, w = 1 for uv.y value
*/

ReadonlyImage atlas_texture : register(t1);
Sampler sampler : register(s0);

[[shader("fragment")]]
float4 FragMain(VertexOut in) : SV_Target {
    return lerp(atlas_texture.Sample(sampler, in.uv), in.color, in.color_factor);
}