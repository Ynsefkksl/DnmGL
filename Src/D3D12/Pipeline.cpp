#include "DnmGL/D3D12/Pipeline.hpp"
#include "DnmGL/D3D12/Shader.hpp"
#include "DnmGL/D3D12/ResourceManager.hpp"
#include "DnmGL/D3D12/ToDxgiFormat.hpp"

namespace DnmGL::D3D12 {
    static void CreateRootSignature(
        ID3D12Device *device, 
        const D3D12::ResourceManager *resource_manager, 
        std::span<const EntryPointInfo *> entry_points, 
        ComPtr<ID3D12RootSignature>& root_signature) {

        const auto descriptor_ranges_srv_uav_cbv = resource_manager->GetDescriptorRangesSRV_UAV_CBV(entry_points);
        const auto descriptor_ranges_sampler = resource_manager->GetDescriptorRangeSampler(entry_points);

        D3D12_ROOT_PARAMETER root_param[2]{};
        root_param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_param[0].DescriptorTable.pDescriptorRanges = descriptor_ranges_srv_uav_cbv.data();
        root_param[0].DescriptorTable.NumDescriptorRanges = descriptor_ranges_srv_uav_cbv.size();
        root_param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        if (descriptor_ranges_sampler.has_value()) {
            root_param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            root_param[1].DescriptorTable.pDescriptorRanges = &descriptor_ranges_sampler.value();
            root_param[1].DescriptorTable.NumDescriptorRanges = 1;
            root_param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
        root_signature_desc.NumParameters = 1 + descriptor_ranges_sampler.has_value();
        root_signature_desc.pParameters = root_param;
        root_signature_desc.NumStaticSamplers = 0;
        root_signature_desc.pStaticSamplers = nullptr;
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> blob, error;
        D3D12SerializeRootSignature(
            &root_signature_desc,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
            &blob,
            &error
        );

        if (error) {
            DnmGLAssert(!error, "D3D12SerializeRootSignature failed: {}", 
                reinterpret_cast<char *>(error->GetBufferPointer()));
        }

        device->CreateRootSignature(
            0,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            IID_PPV_ARGS(&root_signature)
        );
    }

    static constexpr D3D12_FILL_MODE D3D12FillMode(PolygonMode mode) {
        switch (mode) {
            case PolygonMode::eFill: return D3D12_FILL_MODE_SOLID;
            case PolygonMode::eLine: return D3D12_FILL_MODE_WIREFRAME;
        }
    }

    static constexpr D3D12_CULL_MODE D3D12CullMode(CullMode mode) {
        switch (mode) {
            case CullMode::eNone: return D3D12_CULL_MODE_NONE;
            case CullMode::eFront: return D3D12_CULL_MODE_FRONT;
            case CullMode::eBack: return D3D12_CULL_MODE_BACK;
        }
    }

    static constexpr D3D12_COMPARISON_FUNC D3D12ComparisonFunc(CompareOp compare_op) {
        switch (compare_op) {
            case CompareOp::eNone: return D3D12_COMPARISON_FUNC_NONE;
            case CompareOp::eNever: return D3D12_COMPARISON_FUNC_NEVER;
            case CompareOp::eLess: return D3D12_COMPARISON_FUNC_LESS;
            case CompareOp::eEqual: return D3D12_COMPARISON_FUNC_EQUAL;
            case CompareOp::eLessOrEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case CompareOp::eGreater: return D3D12_COMPARISON_FUNC_GREATER;
            case CompareOp::eNotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case CompareOp::eGreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case CompareOp::eAlways: return D3D12_COMPARISON_FUNC_ALWAYS;
        }
    } 

    static constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE D3D12TopologyType(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::ePointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            case PrimitiveTopology::eLineList:
            case PrimitiveTopology::eLineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case PrimitiveTopology::eTriangleList:
            case PrimitiveTopology::eTriangleStrip:
            case PrimitiveTopology::eTriangleFan: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    } 

    GraphicsPipeline::GraphicsPipeline(D3D12::Context& ctx, const DnmGL::GraphicsPipelineDesc& desc) noexcept
        : DnmGL::GraphicsPipeline(ctx, desc) {
        const auto *typed_vertex_shader = static_cast<const D3D12::Shader *>(m_desc.vertex_shader);
        const auto *vertex_entry_point = typed_vertex_shader->GetEntryPoint(m_desc.vertex_entry_point);
        const auto *typed_fragment_shader = static_cast<const D3D12::Shader *>(m_desc.fragment_shader);
        const auto *fragment_entry_point = typed_fragment_shader->GetEntryPoint(m_desc.fragment_entry_point);

        const EntryPointInfo *entry_points[2] = { vertex_entry_point, fragment_entry_point };
        CreateRootSignature(
            D3D12Context->GetDevice(), 
            static_cast<const D3D12::ResourceManager *>(m_desc.resource_manager), 
            entry_points, 
            m_root_signature);

        auto *vertex_shader_blob = typed_vertex_shader->GetShaderBlob(m_desc.vertex_entry_point);
        auto *fragment_shader_blob = typed_fragment_shader->GetShaderBlob(m_desc.fragment_entry_point);

        D3D12_RASTERIZER_DESC rasterizer_desc;
        rasterizer_desc.MultisampleEnable = HasMsaa();
        rasterizer_desc.ForcedSampleCount = 0;
        rasterizer_desc.FillMode = D3D12FillMode(m_desc.polygone_mode);
        rasterizer_desc.CullMode = D3D12CullMode(m_desc.cull_mode);
        rasterizer_desc.FrontCounterClockwise = m_desc.front_face == FrontFace::eCounterClockwise;
        rasterizer_desc.DepthClipEnable = m_desc.depth_test;
        
        D3D12_DEPTH_STENCIL_DESC depth_stencil_desc;
        depth_stencil_desc.DepthEnable = m_desc.depth_test;
        depth_stencil_desc.StencilEnable = m_desc.stencil_test;
        depth_stencil_desc.DepthFunc = D3D12ComparisonFunc(m_desc.depth_test_compare_op);
        depth_stencil_desc.DepthWriteMask = m_desc.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        //TODO:
        // depth_stencil_desc.StencilReadMask = {};
        // depth_stencil_desc.StencilWriteMask = {};
        // depth_stencil_desc.FrontFace = {};
        // depth_stencil_desc.BackFace = {};

        //TODO: fix this
        D3D12_INPUT_LAYOUT_DESC input_desc{};

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{};
        pipeline_desc.pRootSignature = m_root_signature.Get();
        pipeline_desc.VS.pShaderBytecode = vertex_shader_blob->GetBufferPointer();
        pipeline_desc.VS.BytecodeLength = vertex_shader_blob->GetBufferSize();
        pipeline_desc.PS.pShaderBytecode = fragment_shader_blob->GetBufferPointer();
        pipeline_desc.PS.BytecodeLength = fragment_shader_blob->GetBufferSize();
        pipeline_desc.BlendState.RenderTarget->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pipeline_desc.RasterizerState = rasterizer_desc;
        //pipeline_desc.DepthStencilState = depth_stencil_desc;
        pipeline_desc.InputLayout = input_desc;
        pipeline_desc.PrimitiveTopologyType = D3D12TopologyType(m_desc.topology);
        pipeline_desc.NumRenderTargets = m_desc.color_attachment_formats.size();

        for (const auto i : Counter(m_desc.color_attachment_formats.size()))
            pipeline_desc.RTVFormats[i] = ToDxgiFormat(m_desc.color_attachment_formats[i]);

        pipeline_desc.DSVFormat = ToDxgiFormat(m_desc.depth_stencil_format);
        pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC{static_cast<uint32_t>(m_desc.msaa), 0};
        pipeline_desc.SampleMask = UINT_MAX;

        D3D12Context->GetDevice()->CreateGraphicsPipelineState(
            &pipeline_desc, 
            IID_PPV_ARGS(&m_pipeline_state));        
    }

    ComputePipeline::ComputePipeline(D3D12::Context& ctx, const DnmGL::ComputePipelineDesc& desc) noexcept
        : DnmGL::ComputePipeline(ctx, desc) {
        const auto *typed_shader = static_cast<const D3D12::Shader *>(m_desc.shader);
        const auto *entry_point = typed_shader->GetEntryPoint(m_desc.shader_entry_point);

        CreateRootSignature(
            D3D12Context->GetDevice(), 
            static_cast<const D3D12::ResourceManager *>(m_desc.resource_manager), 
            {&entry_point, 1}, 
            m_root_signature);

        auto *shader_blob = typed_shader->GetShaderBlob(entry_point->name);
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
        pipeline_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
        pipeline_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
        pipeline_desc.pRootSignature = m_root_signature.Get();
        
        D3D12Context->GetDevice()->CreateComputePipelineState(
            &pipeline_desc, 
            IID_PPV_ARGS(&m_pipeline_state));
    }
}