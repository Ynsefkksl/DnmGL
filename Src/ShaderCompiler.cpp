#include "DnmGL/DnmGL.hpp"

#include "slang.h"
#include "slang-com-ptr.h"
#include "slang-com-helper.h"

namespace DnmGL {
    constexpr uint32_t spirv_target_index = 0;
    constexpr uint32_t dxil_target_index = 1;
    constexpr uint32_t metal_target_index = 2;

    //TODO: fix the filesystem::path string_view confusion
    //TODO: make ShaderCompiler interface
    //TODO: this might be RHIObject
    class ShaderCompiler {
    public:
        ShaderCompiler();

        const ShaderData &GetShaderData(std::string_view);

        static ShaderData ReadShaderDataFromFile(std::string_view shader);
        static void WriteShaderDataToFile(std::string_view shader, const ShaderData &shader_data);
        ShaderData GetShaderDataFromShader(const std::filesystem::path &path);
    private:
        static std::string ReadShaderFile(const std::filesystem::path &path);

        Slang::ComPtr<slang::IGlobalSession> m_global_session;
        Slang::ComPtr<slang::ISession> m_session;

        std::unordered_map<std::string, ShaderData> m_shaders;
    };

    void Context::CreateShaderCompiler() {
        shader_compiler = new ShaderCompiler;
    }
    
    //TODO: fix return values
    bool Context::CompileShader(std::string_view shader_name) const noexcept {
        shader_compiler->GetShaderDataFromShader(GetShaderPath(shader_name));
        return true;
    }

    bool Context::WriteShaderData(std::string_view shader_name) const noexcept {
        shader_compiler->WriteShaderDataToFile(shader_name, shader_compiler->GetShaderData(shader_name));
        return true;
    }

    void Context::DestroyShaderCompiler() {
        delete shader_compiler;
    }

    static constexpr ResourceType SlangToDnmGL(slang::BindingType type) {
        switch (type) {
            case slang::BindingType::Sampler: return ResourceType::eSampler;
            case slang::BindingType::Texture: return ResourceType::eReadonlyImage;
            case slang::BindingType::ConstantBuffer: return ResourceType::eUniformBuffer;
            case slang::BindingType::RawBuffer:
            case slang::BindingType::TypedBuffer: return ResourceType::eReadonlyBuffer;
            case slang::BindingType::MutableTexture: return ResourceType::eWritableImage;
            case slang::BindingType::MutableRawBuffer:
            case slang::BindingType::MutableTypedBuffer: return ResourceType::eWritableBuffer;
            case slang::BindingType::PushConstant: // return ResourceType::ePushConstant;

            case slang::BindingType::CombinedTextureSampler:
            case slang::BindingType::InputRenderTarget:
            case slang::BindingType::ParameterBlock:
            case slang::BindingType::InlineUniformData:
            case slang::BindingType::Unknown:
            case slang::BindingType::RayTracingAccelerationStructure:
            case slang::BindingType::VaryingInput:
            case slang::BindingType::VaryingOutput:
            case slang::BindingType::ExistentialValue:
            case slang::BindingType::MutableFlag:
            case slang::BindingType::BaseMask:
            case slang::BindingType::ExtMask: return ResourceType::eNone;
        };
    }

    static constexpr ShaderStageBits SlangToDnmGL(SlangStage stage) {
        switch (stage) {
            case SLANG_STAGE_VERTEX: return ShaderStageBits::eVertex;
            case SLANG_STAGE_FRAGMENT: return ShaderStageBits::eFragment;
            case SLANG_STAGE_COMPUTE: return ShaderStageBits::eCompute;

            case SLANG_STAGE_NONE:
            case SLANG_STAGE_HULL:
            case SLANG_STAGE_DOMAIN:
            case SLANG_STAGE_GEOMETRY:
            case SLANG_STAGE_RAY_GENERATION:
            case SLANG_STAGE_INTERSECTION:
            case SLANG_STAGE_ANY_HIT:
            case SLANG_STAGE_CLOSEST_HIT:
            case SLANG_STAGE_MISS:
            case SLANG_STAGE_CALLABLE:
            case SLANG_STAGE_MESH:
            case SLANG_STAGE_AMPLIFICATION:
            case SLANG_STAGE_DISPATCH:
            case SLANG_STAGE_COUNT: return ShaderStageBits::eNone;
        };
    }

    static constexpr std::vector<Resource> GetResources(slang::ProgramLayout *spirv_layout, slang::ProgramLayout *dxil_layout, slang::ProgramLayout *metal_layout) {
        std::vector<Resource> resources;
        resources.reserve(spirv_layout->getParameterCount());
        for (const auto i : Counter(spirv_layout->getParameterCount())) {
            auto *var_layout = spirv_layout->getParameterByIndex(i);
            resources.emplace_back(
                SlangToDnmGL(var_layout->getTypeLayout()->getBindingRangeType(0)),
                spirv_layout->getParameterByIndex(i)->getBindingIndex(),
                dxil_layout->getParameterByIndex(i)->getBindingIndex(),
                metal_layout->getParameterByIndex(i)->getBindingIndex(),
                static_cast<uint32_t>(var_layout->getTypeLayout()->getBindingRangeBindingCount(0)),
                var_layout->getName()
            );
        }

        return resources;
    }

    static constexpr EntryPoint GetEntryPoint(slang::EntryPointReflection *entry_point_refl) {
        return {
            SlangToDnmGL(entry_point_refl->getStage()),
            entry_point_refl->getName()
        };
    }

    static constexpr std::vector<EntryPoint> GetEntryPoints(slang::ProgramLayout *layout) {
        std::vector<EntryPoint> entry_points;
        entry_points.reserve(layout->getEntryPointCount());

        for (const auto i : Counter(layout->getEntryPointCount())) {
            auto *entry_point_refl = layout->getEntryPointByIndex(i); 
            entry_points.emplace_back(
                SlangToDnmGL(entry_point_refl->getStage()),
                entry_point_refl->getName()
            );
        }
        return entry_points;
    }

    static constexpr ShaderType GetShaderType(slang::ProgramLayout *layout) {
        ShaderType out;
        for (const auto i : Counter(layout->getEntryPointCount())) {
            if (i == 0) {
                if (layout->getEntryPointByIndex(i)->getStage() == SLANG_STAGE_COMPUTE)
                    out = ShaderType::eComputeShader;
                else
                    out = ShaderType::eGraphicsShader;
            }

            if (layout->getEntryPointByIndex(i)->getStage() == SLANG_STAGE_COMPUTE)
                assert(out == ShaderType::eComputeShader);
            else
                assert(out == ShaderType::eGraphicsShader);
        }
        return out;
    }

    static std::vector<char> GetShaderCodeForSpirv(Slang::ComPtr<slang::IComponentType> linked_program) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = linked_program->getTargetCode(
                spirv_target_index, 
                out.writeRef());
        }
        
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static std::vector<char> GetShaderCodeForMetal(Slang::ComPtr<slang::IComponentType> linked_program) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = linked_program->getTargetCode(
                metal_target_index, 
                out.writeRef());
        }
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static std::vector<char> GetShaderCodeForDxil(Slang::ComPtr<slang::IComponentType> linked_program, uint32_t entry_point_index) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = linked_program->getEntryPointCode(
                entry_point_index, 
                dxil_target_index, 
                out.writeRef(), 
                diagnostics_blob.writeRef());
            {
                if (result < 0) {
                    std::println("dxil linking failed: {}", std::string_view((char *)diagnostics_blob->getBufferPointer(), diagnostics_blob->getBufferSize()));
                    return {};
                }
            };
        }
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static ShaderReflection GetShaderReflectionFromProgram(Slang::ComPtr<slang::IComponentType> linked_program) {
        return ShaderReflection{
            GetShaderType(linked_program->getLayout(spirv_target_index)),
            GetResources(
                linked_program->getLayout(spirv_target_index), 
                linked_program->getLayout(dxil_target_index), 
                linked_program->getLayout(metal_target_index)),
            GetEntryPoints(linked_program->getLayout())
        };
    }

    static ShaderData GetShaderDataFromProgram(Slang::ComPtr<slang::IComponentType> linked_program) {
        auto shader_data = ShaderData{
            GetShaderReflectionFromProgram(linked_program),
            GetShaderCodeForSpirv(linked_program),
            GetShaderCodeForMetal(linked_program),
        };

        for (uint32_t i{}; auto &[_, name] : shader_data.reflection.entry_points)
            shader_data.dxil_codes.emplace(name, GetShaderCodeForDxil(linked_program, i++));
        
        return shader_data;
    }

    ShaderCompiler::ShaderCompiler() {
        createGlobalSession(m_global_session.writeRef());
        
        slang::SessionDesc session_desc = {};         

        std::array<slang::TargetDesc, 3> target_desc;
        target_desc[spirv_target_index].format = SLANG_SPIRV;
        target_desc[spirv_target_index].profile = m_global_session->findProfile("spirv_1_3");

        //TODO: reduce sm profile
        target_desc[dxil_target_index].format = SLANG_DXIL;
        target_desc[dxil_target_index].profile = m_global_session->findProfile("sm_6_3");

        target_desc[metal_target_index].format = SLANG_METAL;

        std::array<slang::CompilerOptionEntry, 1> options;
        options[0].name = slang::CompilerOptionName::Optimization;
        options[0].value = {slang::CompilerOptionValueKind::Int, 3};

        session_desc.targets = target_desc.data();
        session_desc.targetCount = target_desc.size();

        session_desc.compilerOptionEntries = options.data();
        session_desc.compilerOptionEntryCount = options.size();

        m_global_session->createSession(session_desc, m_session.writeRef());
    }

    std::string ShaderCompiler::ReadShaderFile(const std::filesystem::path &path) {
        std::ifstream file(path);
        DnmGLAssert(bool(file), "there is no {}", path.string());

        return { std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>() };
    }

    ShaderData ShaderCompiler::GetShaderDataFromShader(const std::filesystem::path &path) {
        Slang::ComPtr<slang::IModule> slang_module;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang_module = m_session->loadModuleFromSourceString(
                "Sprite", 
                path.string().c_str(), 
                ReadShaderFile(path).c_str(), 
                diagnostics_blob.writeRef());

            if (!slang_module) {
                std::println("load module error: {}", 
                    std::string_view((const char *)diagnostics_blob->getBufferPointer(), diagnostics_blob->getBufferSize()));
            }
        }

        std::vector<slang::IComponentType *> component_types = {
            slang_module,
        };

        std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points(slang_module->getDefinedEntryPointCount());
        for (const auto i : Counter(slang_module->getDefinedEntryPointCount())) {
            slang_module->getDefinedEntryPoint(i, entry_points[i].writeRef());
            component_types.emplace_back(entry_points[i]);
        }

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            SlangResult result = m_session->createCompositeComponentType(
                component_types.data(),
                component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());
            //TODO: check result
            if (diagnostics_blob) {
                std::println("compese error: {}", std::string_view((char *)diagnostics_blob->getBufferPointer(), diagnostics_blob->getBufferSize()));
            }
        }

        Slang::ComPtr<slang::IComponentType> linked_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            SlangResult result = composed_program->link(
                linked_program.writeRef(),
                diagnostics_blob.writeRef());
            //TODO: check result
            if (diagnostics_blob) {
                std::println("compese error: {}", std::string_view((char *)diagnostics_blob->getBufferPointer(), diagnostics_blob->getBufferSize()));
            }
        }

        return GetShaderDataFromProgram(linked_program);
    }

    const ShaderData &ShaderCompiler::GetShaderData(std::string_view filename) {
        if (const auto string_filename = std::string(filename); 
            m_shaders.contains(string_filename))
            return m_shaders.at(string_filename);
        
        return m_shaders.emplace(filename, GetShaderDataFromShader(filename)).first->second;
    }

    ShaderData ShaderCompiler::ReadShaderDataFromFile(std::string_view path) {
        const auto get_string_in_file = [](std::ifstream &file) noexcept {
            std::string string{};
            char c;
            
            while (file.get(c) && (c != '\0'))
                string += c;
            
            return string;
        };

        
        std::ifstream file(std::filesystem::path(path), std::ios::in | std::ios::binary);
        ShaderData out{};

        // Get shader type
        {
            file.read(reinterpret_cast<char *>(&out.reflection.shader_type), sizeof(uint32_t));
        }
        
        // Get resources
        {
            uint32_t resource_count;
            file.read(reinterpret_cast<char *>(&resource_count), sizeof(uint32_t));
            out.reflection.resources.resize(resource_count);

            for (auto &res : out.reflection.resources) {
                file.read(reinterpret_cast<char *>(&res.type), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.spirv_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.dxil_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.metal_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.resource_count), sizeof(uint32_t));

                res.name = get_string_in_file(file);
            }
        }

        // Get entry points
        {
            uint32_t entry_point_count;
            file.read(reinterpret_cast<char *>(&entry_point_count), sizeof(uint32_t));
            out.reflection.entry_points.resize(entry_point_count);

            for (auto &entry_point : out.reflection.entry_points) {
                file.read(reinterpret_cast<char *>(&entry_point.shader_stage), sizeof(uint32_t));
                entry_point.name = get_string_in_file(file);
            }
        }

        // Get spirv code
        {
            uint32_t shader_size;
            file.read((char *)&shader_size, sizeof(uint32_t));
            out.spirv_code.resize(shader_size);
            file.read(out.spirv_code.data(), shader_size);
        }

        // Get dxil codes
        {
            for (const auto &entry_point : out.reflection.entry_points) {
                uint32_t shader_size;
                file.read((char *)&shader_size, sizeof(uint32_t));

                std::vector<char> dxil_code(shader_size);
                file.read(dxil_code.data(), shader_size);

                out.dxil_codes.emplace(entry_point.name, std::move(dxil_code));
            }
        }

        // Get metal code
        {
            uint32_t shader_size;
            file.read((char *)&shader_size, sizeof(uint32_t));
            out.metal_code.resize(shader_size);
            file.read(out.metal_code.data(), shader_size);
        }
        file.close();

        return out;
    }

    void ShaderCompiler::WriteShaderDataToFile(std::string_view path, const ShaderData &shader_data) {
        std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);

        // shader type
        {
            //shader type is uint8_t
            const uint32_t shader_type = static_cast<uint32_t>(shader_data.reflection.shader_type);
            file.write(reinterpret_cast<const char *>(&shader_type), sizeof(uint32_t));
        }

        // resources
        {
            const uint32_t resource_count = shader_data.reflection.resources.size();
            file.write(reinterpret_cast<const char *>(&resource_count), sizeof(uint32_t));
            for (const auto &res : shader_data.reflection.resources) {
                //type is uint8_t
                const uint32_t buff = static_cast<uint32_t>(res.type);
                file.write(reinterpret_cast<const char *>(&res.type), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&res.spirv_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&res.dxil_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&res.metal_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&res.resource_count), sizeof(uint32_t));
                file.write(res.name.c_str(), res.name.size());
                file.write("\0", 1);
            }
        }

        // entry points
        {
            const uint32_t entry_point_count = shader_data.reflection.entry_points.size();
            file.write(reinterpret_cast<const char *>(&entry_point_count), sizeof(uint32_t));
            for (const auto &entry_point : shader_data.reflection.entry_points) {
                //type is uint8_t
                const uint32_t buff = static_cast<uint32_t>(entry_point.shader_stage);
                file.write(reinterpret_cast<const char *>(&buff), sizeof(uint32_t));
                file.write(entry_point.name.c_str(), entry_point.name.size());
                file.write("\0", 1);
            }
        }

        // spirv code
        {
            const auto code_size = shader_data.spirv_code.size();

            file.write(reinterpret_cast<const char *>(&code_size), sizeof(uint32_t));
            file.write(shader_data.spirv_code.data(), code_size);
        }

        // dxil codes
        {
            for (const auto i : shader_data.reflection.entry_points) {
                const auto code_size = shader_data.dxil_codes.at(i.name).size();

                file.write(reinterpret_cast<const char *>(&code_size), sizeof(uint32_t));
                file.write(shader_data.dxil_codes.at(i.name).data(), code_size);
            }
        }

        // metal code
        {
            const auto code_size = shader_data.metal_code.size();

            file.write(reinterpret_cast<const char *>(&code_size), sizeof(uint32_t));
            file.write(shader_data.metal_code.data(), code_size);
        }

        file.close();
    }
}