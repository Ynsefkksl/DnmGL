#include "DnmGL/D3D12/Pipeline.hpp"
#include "DnmGL/D3D12/Buffer.hpp"
#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/Sampler.hpp"
#include "DnmGL/D3D12/ToDxgiFormat.hpp"

namespace DnmGL::D3D12 {
    static Resource *GetResource(ShaderReflection &reflection, std::string_view resource_name) {
        const auto it = reflection.resources.find(std::string(resource_name));
        return it == reflection.resources.end() ? nullptr : &it->second;
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC GetImageSRV(const D3D12::Image *image, const ImageSubresource &subresource) {
        D3D12_SHADER_RESOURCE_VIEW_DESC out;
        out.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        out.Format = ToDxgiFormat(image->GetDesc().format);
        if (subresource.type == ImageSubresourceType::e1D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            out.Texture1D = D3D12_TEX1D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e2D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            out.Texture2D = D3D12_TEX2D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e2DArray) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            out.Texture2DArray = D3D12_TEX2D_ARRAY_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .FirstArraySlice = subresource.base_layer,
                .ArraySize = subresource.layer_count,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e3D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            out.Texture3D = D3D12_TEX3D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::eCube) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            out.TextureCube = D3D12_TEXCUBE_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        return out;
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC GetImageUAV(const D3D12::Image *image, const ImageSubresource &subresource) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC out;
        out.Format = ToDxgiFormat(image->GetDesc().format);
        if (subresource.type == ImageSubresourceType::e1D) {
            out.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            out.Texture1D = D3D12_TEX1D_UAV{
                subresource.base_mipmap
            };
        }
        else if (subresource.type == ImageSubresourceType::e2D) {
            out.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            out.Texture2D = D3D12_TEX2D_UAV{
                subresource.base_mipmap,
                0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e2DArray) {
            out.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            out.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{
                .MipSlice = subresource.base_mipmap,
                .FirstArraySlice = subresource.base_layer,
                .ArraySize = subresource.layer_count,
                .PlaneSlice = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e3D) {
            out.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            out.Texture3D = D3D12_TEX3D_UAV{
                .MipSlice = subresource.base_mipmap,
                .FirstWSlice = 0,
                .WSize = image->GetDesc().extent.z
            };
        }
        return out;
    }

    static void CreateDescriptorHeaps(ID3D12Device *device, 
        const ShaderReflection &shader_reflection, 
        //out
        ComPtr<ID3D12DescriptorHeap> &descriptor_heap,
        ComPtr<ID3D12DescriptorHeap> &sampler_heap,
        // 0 readonly, 1 writable, 2 uniform, 3 sampler
        std::span<uint32_t, 4> descriptor_counts) {
        
        auto &readonly_resource_count = descriptor_counts[0];
        auto &writable_resource_count = descriptor_counts[1];
        auto &uniform_resource_count = descriptor_counts[2];
        auto &sampler_resource_count = descriptor_counts[3];
        
        for (auto &[_, res] : shader_reflection.resources) {
            if (res.type == ResourceType::eReadonlyBuffer || res.type == ResourceType::eReadonlyImage)
                readonly_resource_count++;
            else if (res.type == ResourceType::eWritableBuffer || res.type == ResourceType::eWritableImage)
                writable_resource_count++;
            else if (res.type == ResourceType::eUniformBuffer)
                uniform_resource_count++;
            else if (res.type == ResourceType::eSampler)
                sampler_resource_count++;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        rtv_heap_desc.NumDescriptors = readonly_resource_count
                                        + writable_resource_count
                                        + uniform_resource_count
                                        ;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&descriptor_heap));

        rtv_heap_desc.NumDescriptors = sampler_resource_count;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&sampler_heap));
    }

    static void CreateRootSignature(
        ID3D12Device *device, 
        ComPtr<ID3D12RootSignature> &root_signature,
        // 0 readonly, 1 writable, 2 uniform, 3 sampler
        std::span<const uint32_t, 4> descriptor_counts) {

        const auto readonly_resource_count = descriptor_counts[0];
        const auto writable_resource_count = descriptor_counts[1];
        const auto uniform_resource_count = descriptor_counts[2];
        const auto sampler_resource_count = descriptor_counts[3];

        std::vector<D3D12_DESCRIPTOR_RANGE> descriptor_ranges_srv_uav_cbv;
        std::optional<D3D12_DESCRIPTOR_RANGE> descriptor_ranges_sampler;

        if (readonly_resource_count != 0)
            descriptor_ranges_srv_uav_cbv.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                readonly_resource_count,
                0,
                0,
                0         
            );
        if (writable_resource_count != 0)
            descriptor_ranges_srv_uav_cbv.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                writable_resource_count,
                0,
                0,
                readonly_resource_count           
            );
        if (uniform_resource_count != 0)
            descriptor_ranges_srv_uav_cbv.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                uniform_resource_count,
                0,
                0,
                readonly_resource_count + writable_resource_count          
            );
        if (sampler_resource_count != 0)
            descriptor_ranges_sampler = {
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
            sampler_resource_count,
            0,
            0,
            0
            };
        

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

        //maybe this just error
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
        CreateDescriptorHeaps(
            D3D12Context->GetDevice(), 
            m_shader_data->reflection, 
            m_descriptor_heap,
            m_sampler_heap, 
            m_resource_counts);

        CreateRootSignature(
            D3D12Context->GetDevice(), 
            m_root_signature, 
            m_resource_counts);

        const void *vertex_shader_code;
        uint32_t vertex_shader_size;

        const void *fragment_shader_code;
        uint32_t fragment_shader_size;

        // GraphicsShader have just vertex and fragment entry point
        for (auto &[_, entry_point] : m_shader_data->reflection.entry_points) {
            if (entry_point.shader_stage == ShaderStageBits::eVertex) {
                vertex_shader_code = m_shader_data->dxil_codes.at(entry_point.name).data();
                vertex_shader_size = m_shader_data->dxil_codes.at(entry_point.name).size();
            }
            else {
                fragment_shader_code = m_shader_data->dxil_codes.at(entry_point.name).data();
                fragment_shader_size = m_shader_data->dxil_codes.at(entry_point.name).size();
            }
        }

        D3D12_RASTERIZER_DESC rasterizer_desc;
        rasterizer_desc.MultisampleEnable = HasMsaa();
        rasterizer_desc.ForcedSampleCount = 0;
        rasterizer_desc.FillMode = D3D12FillMode(m_desc.rasterizer_desc->polygone_mode);
        rasterizer_desc.CullMode = D3D12CullMode(m_desc.rasterizer_desc->cull_mode);
        rasterizer_desc.FrontCounterClockwise = m_desc.rasterizer_desc->front_face == FrontFace::eCounterClockwise;
        rasterizer_desc.DepthClipEnable = m_desc.depth_stencil_desc->depth_test;
        
        D3D12_DEPTH_STENCIL_DESC depth_stencil_desc;
        depth_stencil_desc.DepthEnable = m_desc.depth_stencil_desc->depth_test;
        depth_stencil_desc.StencilEnable = m_desc.depth_stencil_desc->stencil_test;
        depth_stencil_desc.DepthFunc = D3D12ComparisonFunc(m_desc.depth_stencil_desc->depth_test_compare_op);
        depth_stencil_desc.DepthWriteMask = m_desc.depth_stencil_desc->depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        // depth_stencil_desc.StencilReadMask = 0xFF;
        // depth_stencil_desc.StencilWriteMask = 0xFF;
        // depth_stencil_desc.FrontFace = D3D12_DEPTH_STENCILOP_DESC{

        // };
        // depth_stencil_desc.BackFace = D3D12_DEPTH_STENCILOP_DESC{

        // };

        //TODO: fix this
        D3D12_INPUT_LAYOUT_DESC input_desc{};

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{};
        pipeline_desc.pRootSignature = m_root_signature.Get();
        pipeline_desc.VS.pShaderBytecode = vertex_shader_code;
        pipeline_desc.VS.BytecodeLength = vertex_shader_size;
        pipeline_desc.PS.pShaderBytecode = fragment_shader_code;
        pipeline_desc.PS.BytecodeLength = fragment_shader_size;
        pipeline_desc.RasterizerState = rasterizer_desc;
        pipeline_desc.DepthStencilState = depth_stencil_desc;
        pipeline_desc.InputLayout = input_desc;
        pipeline_desc.PrimitiveTopologyType = D3D12TopologyType(m_desc.input_assembly_desc->topology);
        pipeline_desc.NumRenderTargets = m_desc.rasterizer_desc->color_attachment_formats.size();

        for (const auto i : Counter(m_desc.rasterizer_desc->color_attachment_formats.size())) {
            pipeline_desc.RTVFormats[i] = ToDxgiFormat(m_desc.rasterizer_desc->color_attachment_formats[i]);
            pipeline_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            pipeline_desc.BlendState.RenderTarget[i].BlendEnable = m_desc.rasterizer_desc->color_blend;
            pipeline_desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            pipeline_desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            pipeline_desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_SRC_ALPHA; 
            pipeline_desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; 
            pipeline_desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            pipeline_desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        }

        pipeline_desc.DSVFormat = ToDxgiFormat(m_desc.depth_stencil_desc->depth_stencil_format);
        pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC{static_cast<uint32_t>(m_desc.rasterizer_desc->msaa), 0};
        pipeline_desc.SampleMask = UINT_MAX;

        D3D12Context->GetDevice()->CreateGraphicsPipelineState(
            &pipeline_desc, 
            IID_PPV_ARGS(&m_pipeline_state));   
            
        SetPipelineResources();
    }

    ComputePipeline::ComputePipeline(D3D12::Context& ctx, std::string_view shader_name) noexcept
        : DnmGL::ComputePipeline(ctx, shader_name) {
        CreateDescriptorHeaps(
            D3D12Context->GetDevice(), 
            m_shader_data->reflection, 
            m_descriptor_heap,
            m_sampler_heap, 
            m_resource_counts);

        CreateRootSignature(
            D3D12Context->GetDevice(), 
            m_root_signature, 
            m_resource_counts);

        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
        pipeline_desc.pRootSignature = m_root_signature.Get();
            
        // ComputeShader have just compute shader entry points
        for (const auto &[name, code] : m_shader_data->dxil_codes) {
            pipeline_desc.CS.pShaderBytecode = code.data();
            pipeline_desc.CS.BytecodeLength = code.size();

            auto it = m_pipeline_states.emplace(name, nullptr);

            D3D12Context->GetDevice()->CreateComputePipelineState(
                &pipeline_desc, 
                IID_PPV_ARGS(&it.first->second));
        }

        SetPipelineResources();
    }

    void ComputePipeline::ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) {
        const auto *typed_buffer = static_cast<const D3D12::Buffer *>(buffer_desc.buffer);

        if (resource.type == ResourceType::eUniformBuffer) {
            const D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
                .BufferLocation = typed_buffer->GetResource()->GetGPUVirtualAddress() + (buffer_desc.first_element * typed_buffer->GetDesc().element_size),
                .SizeInBytes = (buffer_desc.element_count * typed_buffer->GetDesc().element_size)
            };

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetUniformResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateConstantBufferView(
                    &desc, heap_cpu_handle);   
            }
        }
        else if (resource.type == ResourceType::eReadonlyBuffer) {
            const auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Buffer = D3D12_BUFFER_SRV{
                    .FirstElement = buffer_desc.first_element,
                    .NumElements = buffer_desc.element_count,
                    .StructureByteStride = typed_buffer->GetDesc().element_size,
                    .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                }
            };
            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateShaderResourceView(
                    typed_buffer->GetResource(), &desc, heap_cpu_handle);
            }
        }
        else if (resource.type == ResourceType::eWritableBuffer) {
            const auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = D3D12_BUFFER_UAV{
                    .FirstElement = buffer_desc.first_element,
                    .NumElements = buffer_desc.element_count,
                    .StructureByteStride = typed_buffer->GetDesc().element_size
                }
            };

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetWriteableResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateUnorderedAccessView(
                    typed_buffer->GetResource(), nullptr, &desc, heap_cpu_handle);   
            }
        }
    }

    void ComputePipeline::ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) {
        const auto *typed_image = static_cast<const D3D12::Image *>(image_desc.image);

        if (resource.type == ResourceType::eReadonlyImage) {
            const auto desc = GetImageSRV(typed_image, image_desc.subresource);
            
            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateShaderResourceView(
                    typed_image->GetResource(), &desc, heap_cpu_handle);   
            }
        }
        else if (resource.type == ResourceType::eWritableImage) {
            const auto desc = GetImageUAV(typed_image, image_desc.subresource);

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateUnorderedAccessView(
                    typed_image->GetResource(), nullptr, &desc, heap_cpu_handle);    
            }
        }
    }

    void ComputePipeline::ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) {
        for (const auto i : Counter(resource_count)) {
            const auto heap_cpu_handle = GetSamplerResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
            D3D12Context->GetDevice()->CreateSampler(&static_cast<const D3D12::Sampler *>(sampler)->m_sampler_desc, heap_cpu_handle);
        }
    }

    void GraphicsPipeline::ISetResource(const Resource &resource, const BufferResourceDesc &buffer_desc, uint16_t resource_index, uint16_t resource_count) {
        const auto *typed_buffer = static_cast<const D3D12::Buffer *>(buffer_desc.buffer);

        if (resource.type == ResourceType::eUniformBuffer) {
            const D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
                .BufferLocation = typed_buffer->GetResource()->GetGPUVirtualAddress() + (buffer_desc.first_element * typed_buffer->GetDesc().element_size),
                .SizeInBytes = (buffer_desc.element_count * typed_buffer->GetDesc().element_size)
            };

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetUniformResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateConstantBufferView(
                    &desc, heap_cpu_handle);   
            }
        }
        else if (resource.type == ResourceType::eReadonlyBuffer) {
            const auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Buffer = D3D12_BUFFER_SRV{
                    .FirstElement = buffer_desc.first_element,
                    .NumElements = buffer_desc.element_count,
                    .StructureByteStride = typed_buffer->GetDesc().element_size,
                    .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                }
            };
            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateShaderResourceView(
                    typed_buffer->GetResource(), &desc, heap_cpu_handle);
            }
        }
        else if (resource.type == ResourceType::eWritableBuffer) {
            const auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = D3D12_BUFFER_UAV{
                    .FirstElement = buffer_desc.first_element,
                    .NumElements = buffer_desc.element_count,
                    .StructureByteStride = typed_buffer->GetDesc().element_size
                }
            };

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetWriteableResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateUnorderedAccessView(
                    typed_buffer->GetResource(), nullptr, &desc, heap_cpu_handle);   
            }
        }
    }

    void GraphicsPipeline::ISetResource(const Resource &resource, const ImageResourceDesc &image_desc, uint16_t resource_index, uint16_t resource_count) {
        const auto *typed_image = static_cast<const D3D12::Image *>(image_desc.image);

        if (resource.type == ResourceType::eReadonlyImage) {
            const auto desc = GetImageSRV(typed_image, image_desc.subresource);
            
            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateShaderResourceView(
                    typed_image->GetResource(), &desc, heap_cpu_handle);   
            }
        }
        else if (resource.type == ResourceType::eWritableImage) {
            const auto desc = GetImageUAV(typed_image, image_desc.subresource);

            for (const auto i : Counter(resource_count)) {
                const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
                D3D12Context->GetDevice()->CreateUnorderedAccessView(
                    typed_image->GetResource(), nullptr, &desc, heap_cpu_handle);    
            }
        }
    }

    void GraphicsPipeline::ISetResource(const Resource &resource, const DnmGL::Sampler *sampler, uint16_t resource_index, uint16_t resource_count) {
        for (const auto i : Counter(resource_count)) {
            const auto heap_cpu_handle = GetSamplerResourceHeapCpuHandle(resource.dxil_index, resource_index + i);
            D3D12Context->GetDevice()->CreateSampler(&static_cast<const D3D12::Sampler *>(sampler)->m_sampler_desc, heap_cpu_handle);
        }
    }
}