#include "DnmGL/D3D12/Shader.hpp"

#include <d3d12shader.h>
#include <dxcapi.h>
#include <fstream>

namespace DnmGL::D3D12 {
    std::string_view ShaderInputTypeToString(D3D_SHADER_INPUT_TYPE type) {
        switch (type) {
            case D3D_SIT_CBUFFER: return "D3D_SIT_CBUFFER";
            case D3D_SIT_TBUFFER: return "D3D_SIT_TBUFFER";
            case D3D_SIT_TEXTURE: return "D3D_SIT_TEXTURE";
            case D3D_SIT_SAMPLER: return "D3D_SIT_SAMPLER";
            case D3D_SIT_UAV_RWTYPED: return "D3D_SIT_UAV_RWTYPED";
            case D3D_SIT_STRUCTURED: return "D3D_SIT_STRUCTURED";
            case D3D_SIT_UAV_RWSTRUCTURED: return "D3D_SIT_UAV_RWSTRUCTURED";
            case D3D_SIT_BYTEADDRESS: return "D3D_SIT_BYTEADDRESS";
            case D3D_SIT_UAV_RWBYTEADDRESS: return "D3D_SIT_UAV_RWBYTEADDRESS";
            case D3D_SIT_UAV_APPEND_STRUCTURED: return "D3D_SIT_UAV_APPEND_STRUCTURED";
            case D3D_SIT_UAV_CONSUME_STRUCTURED: return "D3D_SIT_UAV_CONSUME_STRUCTURED";
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: return "D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER";
            case D3D_SIT_RTACCELERATIONSTRUCTURE: return "D3D_SIT_RTACCELERATIONSTRUCTURE";
            case D3D_SIT_UAV_FEEDBACKTEXTURE: return "D3D_SIT_UAV_FEEDBACKTEXTURE";
        }
    }

    static constexpr ShaderStageBits GetShaderStage(D3D12_SHADER_VERSION_TYPE shader_verison_type) {
        switch (shader_verison_type) {
            case D3D12_SHVER_PIXEL_SHADER: return ShaderStageBits::eFragment;
            case D3D12_SHVER_VERTEX_SHADER: return ShaderStageBits::eVertex;
            case D3D12_SHVER_COMPUTE_SHADER: return ShaderStageBits::eCompute;
            default: return ShaderStageBits::eNone;
        }
    }

    static constexpr ResourceType GetResourceType(const _D3D_SHADER_INPUT_TYPE &type) {
        switch (type) {
            case D3D_SIT_CBUFFER: return ResourceType::eUniformBuffer;
            case D3D_SIT_TEXTURE: return ResourceType::eReadonlyImage;
            case D3D_SIT_SAMPLER: return ResourceType::eSampler;
            case D3D_SIT_UAV_RWTYPED: return ResourceType::eWritableImage;
            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS: return ResourceType::eReadonlyBuffer;
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: return ResourceType::eWritableBuffer;

            case D3D_SIT_TBUFFER:
            case D3D_SIT_RTACCELERATIONSTRUCTURE:
            case D3D_SIT_UAV_FEEDBACKTEXTURE: return ResourceType::eNone;
        }
    }

    static std::vector<uint8_t> LoadShaderFile(const std::filesystem::path& file_path) {
        std::ifstream file(file_path, std::ios::ate | std::ios::binary);

        const auto file_size = file.tellg();
        std::vector<uint8_t> buffer(file_size);

        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        file.close();

        return std::move(buffer);
    }

    static std::wstring ToWideString(std::string_view input) {
        if (input.empty()) return std::wstring();

        const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &input[0], (int)input.size(), NULL, 0);
        
        std::wstring wstrTo(size_needed, 0);
        
        MultiByteToWideChar(CP_UTF8, 0, &input[0], (int)input.size(), &wstrTo[0], size_needed);
        
        return wstrTo;
    }

    Shader::Shader(D3D12::Context& ctx, std::string_view filename)
        : DnmGL::Shader(ctx, filename) {
        const auto shader_file = LoadShaderFile(context->GetShaderPath(filename));
        
        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcContainerReflection> container_reflection;
        ComPtr<IDxcLinker> linker;
        
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection));
        DxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(&linker));
        
        ComPtr<IDxcBlobEncoding> blob_encoding;

        utils->CreateBlob(shader_file.data(), shader_file.size(), 
            CP_UTF8, &blob_encoding);

        container_reflection->Load(blob_encoding.Get());
        linker->RegisterLibrary(L"lib", blob_encoding.Get());
        
        uint32_t part_index;
        auto hr = container_reflection->FindFirstPartKind(DXC_PART_DXIL, &part_index);
        if (FAILED(hr)) {
            //TODO: better error
            std::println("IDxcContainerReflection::FindFirstPartKind failed: {}", HresultToString(hr));
            return;
        }

        ComPtr<ID3D12LibraryReflection> reflection{};
        hr = container_reflection->GetPartReflection(part_index, 
            IID_PPV_ARGS(&reflection));

        if (FAILED(hr)) {
            //TODO: better error
            std::println("ID3D12LibraryReflection::GetPartReflection failed: {}", HresultToString(hr));
            return;
        }

        D3D12_LIBRARY_DESC lib;
        reflection->GetDesc(&lib);

        m_entry_point_infos.reserve(lib.FunctionCount);
        for (const auto i : Counter(lib.FunctionCount)) {
            auto* func_reflection = reflection->GetFunctionByIndex(i);
            D3D12_FUNCTION_DESC func_desc{};
            func_reflection->GetDesc(&func_desc);

            const auto shader_stage = GetShaderStage(D3D12_SHADER_VERSION_TYPE(D3D12_SHVER_GET_TYPE(func_desc.Version)));
            if (shader_stage == ShaderStageBits::eNone)
                continue;

            auto& entry_point = m_entry_point_infos.emplace_back(
                func_desc.Name,
                shader_stage,
                std::vector<BindingInfo>{},
                std::vector<BindingInfo>{},
                std::vector<BindingInfo>{},
                std::vector<BindingInfo>{}
            );

            for (const auto j : Counter(func_desc.BoundResources)) {
                D3D12_SHADER_INPUT_BIND_DESC resource_desc{};
                func_reflection->GetResourceBindingDesc(j, &resource_desc);

                const auto resource_type= GetResourceType(resource_desc.Type);

                DnmGLAssert(resource_type != ResourceType::eNone, 
                    "unsupported resource type; entry point: {}, binding: {}, d3d12 type: {}", 
                    func_desc.Name, resource_desc.BindPoint, ShaderInputTypeToString(resource_desc.Type));

                if (resource_type == ResourceType::eReadonlyImage
                || resource_type == ResourceType::eReadonlyBuffer) {
                    entry_point.readonly_resources.emplace_back(
                        resource_desc.BindPoint,
                        resource_desc.BindCount,
                        resource_type
                    );
                }
                else if (resource_type == ResourceType::eWritableImage
                || resource_type == ResourceType::eWritableBuffer) {
                    entry_point.writable_resources.emplace_back(
                        resource_desc.BindPoint,
                        resource_desc.BindCount,
                        resource_type
                    );
                }
                else if (resource_type == ResourceType::eUniformBuffer) {
                    entry_point.uniform_buffer_resources.emplace_back(
                        resource_desc.BindPoint,
                        resource_desc.BindCount,
                        resource_type
                    );
                }
                else if (resource_type == ResourceType::eSampler) {
                    entry_point.sampler_resources.emplace_back(
                        resource_desc.BindPoint,
                        resource_desc.BindCount,
                        resource_type
                    );
                }
            }

            const auto wide_func_name = ToWideString(func_desc.Name);

            LPCWSTR arguments[] = { L"-O3" };  
            const LPCWSTR libs[1] = {L"lib"};
            LPCWSTR target_profile{};
            switch (entry_point.shader_stage) {
                case ShaderStageBits::eNone: std::unreachable();
                case ShaderStageBits::eVertex: target_profile = L"vs_6_0"; break;
                case ShaderStageBits::eFragment: target_profile = L"ps_6_0"; break;
                case ShaderStageBits::eCompute: target_profile = L"cs_6_0"; break;
            }
            IDxcOperationResult *op_result;
            linker->Link(
                wide_func_name.c_str(),
                target_profile, 
                libs, 
                1, 
                arguments, 
                1,
                &op_result);

            IDxcBlob *blob = nullptr;
            IDxcBlobEncoding *error = nullptr;
            op_result->GetResult(&blob);
            op_result->GetErrorBuffer(&error);

            if (!blob) {
                op_result->GetErrorBuffer(&error);
                BOOL isKnown = FALSE;
                UINT32 codePage = 0;
                hr = error->GetEncoding(&isKnown, &codePage);

                if (SUCCEEDED(hr) && isKnown) {
                    if (codePage == CP_UTF8) {
                        //TODO: handle error
                        std::println("{}", std::string_view(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize()));
                    } 
                    else if (codePage == 1200) {
                    }
                    else if (codePage == CP_ACP) {
                    }
                }
            }
            m_shaders.emplace(func_desc.Name, blob);
        }
    }
}