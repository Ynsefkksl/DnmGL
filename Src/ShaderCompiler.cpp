#include "DnmGL/DnmGL.hpp"

#include "slang.h"
#include "slang-com-ptr.h"
#include "slang-com-helper.h"

//slang reflection sucks
#include "spirv-reflect/spirv_reflect.h"

#include <iostream>

namespace DnmGL {
    constexpr uint32_t spirv_target_index = 0;
    constexpr uint32_t dxil_target_index = 1;
    constexpr uint32_t metal_target_index = 2;

    static constexpr void IsValidShaderData(const ShaderData &shader_data) {
        //TODO: find error description
        if (shader_data.reflection.entry_points.size() != shader_data.dxil_codes.size())
            throw std::runtime_error("shader invalid");

        bool has_fragment_shader = false;
        bool has_vertex_shader = false;
        bool has_compute_shader = false;

        for (const auto &entry_point : shader_data.reflection.entry_points | std::ranges::views::values) {
            //TODO: find error description
            if (!shader_data.dxil_codes.contains(entry_point.name)) throw std::runtime_error("shader invalid");

            switch (entry_point.shader_stage) {
                case ShaderStageBits::eNone: throw std::runtime_error("entry point doesn't have shader stage, entry point: " + entry_point.name);
                case ShaderStageBits::eVertex: has_vertex_shader = true; break;
                case ShaderStageBits::eFragment: has_fragment_shader = true; break;
                case ShaderStageBits::eCompute: has_compute_shader = true; break;
            }
        }

        if (!((has_fragment_shader && has_vertex_shader) && !has_compute_shader)) {
            throw std::runtime_error("shader file must contain either exactly one vertex and one fragment shader, or only compute shaders");
        }
    }

    //TODO: fix the filesystem::path string_view confusion
    //TODO: make ShaderCompiler interface
    //TODO: this must be RHIObject
    class ShaderCompiler {
    public:
        ShaderCompiler();

        const ShaderData &GetShaderData(std::string_view shader_name);

        static void WriteShaderDataToFile(const std::filesystem::path &path, const ShaderData &shader_data);
        const ShaderData *ReadShaderDataFromFile(const std::filesystem::path& path);
        const ShaderData* GetShaderDataFromShader(const std::filesystem::path& path);

        [[nodiscard]] bool ShaderIsExits(const std::string_view shader_name) const noexcept {
            return m_shaders.contains(std::string(shader_name));
        }
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
    std::optional<std::string> Context::CompileShader(const std::string_view shader_name) const noexcept {
        try {
            shader_compiler->GetShaderDataFromShader(GetShaderPath(shader_name));
            return std::nullopt;
        } catch (const std::exception &e) {
            Message(std::format("compiling error: {}", e.what()), MessageType::eShaderCompilationFailed);
            return e.what();
        }
    }

    std::optional<std::string> Context::WriteShaderData(const std::string_view shader_name) const noexcept {
        DnmGL::ShaderCompiler::WriteShaderDataToFile(GetShaderDirectory() / shader_name += ".dnmShader", shader_compiler->GetShaderData(shader_name));
        return std::nullopt;
    }

    std::optional<std::string> Context::ReadOrCompileShader(const std::string_view shader_name) const noexcept {
        try {
            if (std::filesystem::exists((GetShaderDirectory() / shader_name += ".dnmShader"))) {
                shader_compiler->ReadShaderDataFromFile(GetShaderDirectory() / shader_name += ".dnmShader");
            }

            shader_compiler->GetShaderDataFromShader({});
            return std::nullopt;
        } catch (const std::exception &e) {
            return e.what();
        }
    }

    std::expected<const ShaderData*, std::string> Context::GetShaderData(const std::string_view shader_name) const noexcept {
        try {
            if (shader_compiler->ShaderIsExits(shader_name))
                shader_compiler->GetShaderData(shader_name);

            if (std::filesystem::exists((GetShaderDirectory() / shader_name += ".dnmShader")))
                return shader_compiler->ReadShaderDataFromFile(GetShaderDirectory() / shader_name += ".dnmShader");

            if (std::filesystem::exists((GetShaderDirectory() / shader_name += ".slang")))
                return shader_compiler->GetShaderDataFromShader((GetShaderDirectory() / shader_name += ".slang"));

            throw std::runtime_error("shader missing");
        } catch (const std::exception &e) {
            return std::unexpected(e.what());
        }
    }

    void Context::DestroyShaderCompiler() const {
        delete shader_compiler;
    }

    static constexpr ResourceType SlangToDnmGL(const slang::BindingType type) {
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

            default: return ResourceType::eNone;
        };
    }

    static constexpr ShaderStageBits SlangToDnmGL(const SlangStage stage) {
        switch (stage) {
            case SLANG_STAGE_VERTEX: return ShaderStageBits::eVertex;
            case SLANG_STAGE_FRAGMENT: return ShaderStageBits::eFragment;
            case SLANG_STAGE_COMPUTE: return ShaderStageBits::eCompute;

            default: return ShaderStageBits::eNone;
        };
    }

    static constexpr ShaderStageBits SpvReflectToDnmGL(const SpvReflectShaderStageFlagBits stage) {
        switch (stage) {
            case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: return ShaderStageBits::eVertex;
            case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: return ShaderStageBits::eFragment;
            case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: return ShaderStageBits::eCompute;
            default: return ShaderStageBits::eNone;
        };
    }

    static std::unordered_map<std::string, Resource> GetResources(slang::ProgramLayout *spirv_layout, slang::ProgramLayout *dxil_layout, slang::ProgramLayout *metal_layout) {
        std::unordered_map<std::string, Resource> resources;
        resources.reserve(spirv_layout->getParameterCount());
        for (const auto i : Counter(spirv_layout->getParameterCount())) {
            auto *var_layout = spirv_layout->getParameterByIndex(i);

            decltype(Resource::create_desc) create_desc = std::nullopt;
            const auto type = SlangToDnmGL(var_layout->getTypeLayout()->getBindingRangeType(0));

            for (const auto j : Counter(var_layout->getVariable()->getUserAttributeCount())) {
                auto *attrib = var_layout->getVariable()->getUserAttributeByIndex(j);
                const auto attrib_name = std::string(attrib->getName());
                if (attrib_name == "BufferDesc") {
                    if (!(type == ResourceType::eReadonlyBuffer
                    || type == ResourceType::eWritableBuffer
                    || type == ResourceType::eUniformBuffer)) throw std::runtime_error("resource type and create desc incompatible, resource: " + std::string(var_layout->getName()));
                    
                    BufferDesc desc;

                    int buff;
                    attrib->getArgumentValueInt(0, &buff);
                    desc.memory_type = static_cast<MemoryType>(buff);

                    attrib->getArgumentValueInt(1, &buff);
                    desc.memory_host_access = static_cast<MemoryHostAccess>(buff);

                    attrib->getArgumentValueInt(2, &buff);
                    desc.usage_flags = static_cast<BufferUsageFlags>(buff);

                    attrib->getArgumentValueInt(3, &buff);
                    desc.element_count = buff;

                    desc.element_size = var_layout->getTypeLayout()->getElementTypeLayout()->getStride();

                    create_desc = desc;
                    break;
                }
                else if (attrib_name == "ImageDesc") {
                    if (!(type == ResourceType::eReadonlyImage
                    || type == ResourceType::eWritableImage)) throw std::runtime_error(
                        "resource type and create desc incompatible, resource: " + std::string(var_layout->getName()));

                    ImageDesc desc;

                    int buff;
                    attrib->getArgumentValueInt(0, &buff);
                    desc.extent.x = buff;

                    attrib->getArgumentValueInt(1, &buff);
                    desc.extent.y = buff;

                    attrib->getArgumentValueInt(2, &buff);
                    desc.extent.z = buff;

                    attrib->getArgumentValueInt(3, &buff);
                    desc.format = static_cast<ImageFormat>(buff);

                    attrib->getArgumentValueInt(4, &buff);
                    desc.usage_flags = static_cast<ImageUsageFlags>(buff);

                    attrib->getArgumentValueInt(5, &buff);
                    desc.type = static_cast<ImageType>(buff);

                    attrib->getArgumentValueInt(6, &buff);
                    desc.mipmap_levels = buff;

                    attrib->getArgumentValueInt(7, &buff);
                    desc.sample_count = static_cast<SampleCount>(buff);

                    create_desc = desc;
                    break;
                }
                else if (attrib_name == "SamplerDesc") {
                    if (type != ResourceType::eReadonlyImage) throw std::runtime_error(
                        "resource type and create desc incompatible, resource: " + std::string(var_layout->getName()));

                    SamplerDesc desc;

                    int buff;
                    attrib->getArgumentValueInt(0, &buff);
                    desc.mipmap_mode = static_cast<SamplerMipmapMode>(buff);

                    attrib->getArgumentValueInt(1, &buff);
                    desc.compare_op = static_cast<CompareOp>(buff);

                    attrib->getArgumentValueInt(2, &buff);
                    desc.filter = static_cast<SamplerFilter>(buff);

                    attrib->getArgumentValueInt(3, &buff);
                    desc.address_mode_u = static_cast<SamplerAddressMode>(buff);

                    attrib->getArgumentValueInt(4, &buff);
                    desc.address_mode_v = static_cast<SamplerAddressMode>(buff);

                    attrib->getArgumentValueInt(5, &buff);
                    desc.address_mode_w = static_cast<SamplerAddressMode>(buff);

                    create_desc = desc;
                    break;
                }
            }

            resources.emplace(
                var_layout->getName(),
                Resource {
                    type,
                    spirv_layout->getParameterByIndex(i)->getBindingIndex(),
                    dxil_layout->getParameterByIndex(i)->getBindingIndex(),
                    metal_layout->getParameterByIndex(i)->getBindingIndex(),
                    static_cast<uint32_t>(var_layout->getTypeLayout()->getBindingRangeBindingCount(0)),
                    var_layout->getName(),
                    create_desc
                }
            );
        }

        return resources;
    }

    static constexpr EntryPoint GetEntryPoint(slang::EntryPointReflection *entry_point_refl) {
        return {
            entry_point_refl->getName(),
            SlangToDnmGL(entry_point_refl->getStage()),
        };
    }

    static std::unordered_map<std::string, EntryPoint> GetEntryPoints(const std::vector<char> &spirv_code) {
        const spv_reflect::ShaderModule shader_module(spirv_code.size(), (uint32_t *)spirv_code.data());

        std::unordered_map<std::string, EntryPoint> entry_points;
        entry_points.reserve(shader_module.GetEntryPointCount());

        for (const auto i : Counter(shader_module.GetEntryPointCount())) {
            const auto entry_point_name = std::string(shader_module.GetEntryPointName(i));
            EntryPoint entry_point{};
            entry_point.name = entry_point_name;
            entry_point.shader_stage = SpvReflectToDnmGL(shader_module.GetEntryPointShaderStage(i));

            uint32_t count;
            shader_module.EnumerateEntryPointDescriptorBindings(entry_point_name.c_str(), &count, nullptr);
            std::vector<SpvReflectDescriptorBinding *> bindings(count);
            shader_module.EnumerateEntryPointDescriptorBindings(entry_point_name.c_str(), &count, bindings.data());
            for (const auto *binding : bindings) {
                entry_point.used_resources.emplace_back(binding->name);
            }

            entry_points.emplace(entry_point_name, entry_point);
        }

        return entry_points;
    }

    //TODO: make some way
    static void GetEntryPointAttributes(slang::ProgramLayout *layout,
        std::optional<DepthStencilDesc> &depth_stencil_desc,
        std::optional<InputAssemblyDesc> &input_assembly_desc,
        std::optional<RasterizerDesc> &rasterizer_desc) {

        // for (const auto i : Counter(layout->getEntryPointCount())) {
        //     auto *entry_point = layout->getEntryPointByIndex(i);
        //     const auto stage = entry_point->getStage();
        //
        //     if (stage == SLANG_STAGE_VERTEX) {
        //         for (const auto j : Counter(entry_point->getFunction()->getUserAttributeCount())) {
        //             auto *attrib = entry_point->getFunction()->getUserAttributeByIndex(j);
        //             if (const auto attrib_name = std::string(attrib->getName());
        //                 attrib_name == "InputAssemblyDesc") {
        //
        //                 std::vector<VertexBinding> vertex_bindings;
        //                 for (const auto k : Counter(8)) {
        //                     auto *type = attrib->getArgumentType(0)->getElementType();
        //                     std::println("{}", type->getUserAttributeCount());
        //                 }
        //
        //                 int value;
        //                 attrib->getArgumentValueInt(0, &value);
        //
        //                 attrib->getArgumentValueInt(1, &value);
        //
        //                 input_assembly_desc.emplace(
        //                     vertex_bindings,
        //                     static_cast<PrimitiveTopology>(value)
        //                     );
        //                 }
        //         }
        //     }
        //     else if (stage == SLANG_STAGE_FRAGMENT) {
        //
        //     }
        // }
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

    static std::vector<char> GetShaderCodeForSpirv(const Slang::ComPtr<slang::IComponentType> &linked_program) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            linked_program->getTargetCode(
                spirv_target_index, 
                out.writeRef());
        }
        
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static std::vector<char> GetShaderCodeForMetal(const Slang::ComPtr<slang::IComponentType> &linked_program) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            linked_program->getTargetCode(
                metal_target_index, 
                out.writeRef());
        }
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static std::vector<char> GetShaderCodeForDxil(const Slang::ComPtr<slang::IComponentType> &linked_program, const uint32_t entry_point_index) {
        Slang::ComPtr<slang::IBlob> out;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            const auto result = linked_program->getEntryPointCode(
                entry_point_index,
                dxil_target_index,
                out.writeRef(),
                diagnostics_blob.writeRef());
            if (result < 0) {
                std::println("dxil linking failed: {}", std::string_view((char *)diagnostics_blob->getBufferPointer(), diagnostics_blob->getBufferSize()));
                return {};
            }
        }
        return {(char *)out->getBufferPointer(), (char *)(out->getBufferPointer()) + out->getBufferSize()};
    }

    static ShaderReflection GetShaderReflectionFromProgram(const Slang::ComPtr<slang::IComponentType> &linked_program, const std::vector<char> &spirv_code) {
        return ShaderReflection{
            GetShaderType(linked_program->getLayout(spirv_target_index)),
            GetResources(
                linked_program->getLayout(spirv_target_index), 
                linked_program->getLayout(dxil_target_index), 
                linked_program->getLayout(metal_target_index)),
                GetEntryPoints(spirv_code)
        };
    }

    static ShaderData GetShaderDataFromProgram(const Slang::ComPtr<slang::IComponentType> &linked_program) {
        const auto spirv_code = GetShaderCodeForSpirv(linked_program);
        auto shader_data = ShaderData{
            GetShaderReflectionFromProgram(linked_program, spirv_code),
            spirv_code,
            GetShaderCodeForMetal(linked_program),
        };

        GetEntryPointAttributes(linked_program->getLayout(spirv_target_index),
            shader_data.reflection.depth_stencil_desc,
            shader_data.reflection.input_assembly_desc,
            shader_data.reflection.rasterizer_desc
        );

        for (uint32_t i{}; const auto& name : shader_data.reflection.entry_points | std::views::keys)
            shader_data.dxil_codes.emplace(name, GetShaderCodeForDxil(linked_program, i++));
        
        IsValidShaderData(shader_data);

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

        return { std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>() };
    }

    const ShaderData* ShaderCompiler::GetShaderDataFromShader(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) throw std::runtime_error("file not found, file path: " + path.string());

        Slang::ComPtr<slang::IModule> slang_module;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;

            slang_module = m_session->loadModuleFromSourceString(
                path.filename().stem().string().c_str(), 
                path.string().c_str(), 
                ReadShaderFile(path).c_str(), 
                diagnostics_blob.writeRef());

            if (!slang_module) {
                std::println("load module error: {}", 
                    std::string_view(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
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
            m_session->createCompositeComponentType(
                component_types.data(),
                component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());
            //TODO: check result
            if (diagnostics_blob) {
                std::println("compose error: {}",
                             std::string_view((char*)diagnostics_blob->getBufferPointer(),
                                              diagnostics_blob->getBufferSize()));
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
                std::println("compose error: {}",
                             std::string_view((char*)diagnostics_blob->getBufferPointer(),
                                              diagnostics_blob->getBufferSize()));
            }
        }

        return &m_shaders.emplace(path.stem().string(), GetShaderDataFromProgram(linked_program)).first->second;
    }

    const ShaderData &ShaderCompiler::GetShaderData(const std::string_view shader_name) {
        const auto string_shader_name = std::string(shader_name);
        if (m_shaders.contains(string_shader_name))
            return m_shaders.at(string_shader_name);

        throw std::runtime_error("shader not found: " + string_shader_name);
    }

    const ShaderData *ShaderCompiler::ReadShaderDataFromFile(const std::filesystem::path& path) {
        const auto get_string_in_file = [](std::ifstream &file) noexcept {
            std::string string{};
            char c;
            
            while (file.get(c) && (c != '\0'))
                string += c;
            
            return string;
        };
        
        std::ifstream file(path, std::ios::in | std::ios::binary);
        ShaderData out{};

        // Get shader type
        {
            file.read(reinterpret_cast<char *>(&out.reflection.shader_type), sizeof(uint32_t));
        }
        
        // Get resources
        {
            uint32_t resource_count;
            file.read(reinterpret_cast<char *>(&resource_count), sizeof(uint32_t));
            out.reflection.resources.reserve(resource_count);

            for ([[maybe_unused]] auto _ : Counter(resource_count)) {
                Resource res;
                file.read(reinterpret_cast<char *>(&res.type), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.spirv_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.dxil_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.metal_index), sizeof(uint32_t));
                file.read(reinterpret_cast<char *>(&res.resource_count), sizeof(uint32_t));
                uint32_t create_desc_index;
                file.read(reinterpret_cast<char *>(&create_desc_index), sizeof(uint32_t));
                if (create_desc_index == 1) {
                    BufferDesc create_desc;
                    file.read(reinterpret_cast<char *>(&create_desc), sizeof(create_desc));
                    res.create_desc = create_desc;
                }
                else if (create_desc_index == 2) {
                    ImageDesc create_desc;
                    file.read(reinterpret_cast<char *>(&create_desc), sizeof(create_desc));
                    res.create_desc = create_desc;
                }
                else if (create_desc_index == 3) {
                    SamplerDesc create_desc;
                    file.read(reinterpret_cast<char *>(&create_desc), sizeof(create_desc));
                    res.create_desc = create_desc;
                }
                file.read(reinterpret_cast<char *>(&res.create_desc), sizeof(res.create_desc));
                res.name = get_string_in_file(file);
                out.reflection.resources.emplace(res.name, res);
            }
        }

        // Get entry points
        {
            uint32_t entry_point_count;
            file.read(reinterpret_cast<char *>(&entry_point_count), sizeof(uint32_t));
            out.reflection.entry_points.reserve(entry_point_count);

            for ([[maybe_unused]] auto _ : Counter(entry_point_count)) {
                EntryPoint entry_point;
                file.read(reinterpret_cast<char *>(&entry_point.shader_stage), sizeof(uint32_t));
                uint32_t used_resource_count;
                file.read(reinterpret_cast<char *>(&used_resource_count), sizeof(uint32_t));
                out.reflection.entry_points.reserve(used_resource_count);
                for ([[maybe_unused]] auto _unused : Counter(used_resource_count)) {
                    entry_point.used_resources.emplace_back(get_string_in_file(file));
                }
                entry_point.name = get_string_in_file(file);

                out.reflection.entry_points.emplace(entry_point.name, entry_point);
            }
        }

        // Get spirv code
        {
            uint32_t shader_size;
            file.read(reinterpret_cast<char*>(&shader_size), sizeof(uint32_t));
            out.spirv_code.resize(shader_size);
            file.read(out.spirv_code.data(), shader_size);
        }

        // Get dxil codes
        {
            for (const auto& name : out.reflection.entry_points | std::views::keys) {
                uint32_t shader_size;
                file.read(reinterpret_cast<char*>(&shader_size), sizeof(uint32_t));

                std::vector<char> dxil_code(shader_size);
                file.read(dxil_code.data(), shader_size);

                out.dxil_codes.emplace(name, std::move(dxil_code));
            }
        }

        // Get metal code
        {
            uint32_t shader_size;
            file.read(reinterpret_cast<char*>(&shader_size), sizeof(uint32_t));
            out.metal_code.resize(shader_size);
            file.read(out.metal_code.data(), shader_size);
        }
        file.close();

        {
            IsValidShaderData(out);
            return &m_shaders.emplace(path.stem().string(), out).first->second;
        }
    }

    void ShaderCompiler::WriteShaderDataToFile(const std::filesystem::path &path, const ShaderData &shader_data) {

        std::ofstream file(path, std::ios::binary | std::ios::trunc);

        // shader type
        {
            //shader type is uint8_t
            const auto shader_type = static_cast<uint32_t>(shader_data.reflection.shader_type);
            file.write(reinterpret_cast<const char *>(&shader_type), sizeof(uint32_t));
        }

        // resources
        {
            const uint32_t global_resource_count = shader_data.reflection.resources.size();
            file.write(reinterpret_cast<const char *>(&global_resource_count), sizeof(uint32_t));
            for (const auto& [type, spirv_index, dxil_index, metal_index, resource_count, name, create_desc] : shader_data.reflection.resources | std::views::values) {
                //type is uint8_t
                file.write(reinterpret_cast<const char *>(&type), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&spirv_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&dxil_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&metal_index), sizeof(uint32_t));
                file.write(reinterpret_cast<const char *>(&resource_count), sizeof(uint32_t));

                const auto create_desc_index = create_desc.index();
                file.write(reinterpret_cast<const char *>(&create_desc_index), sizeof(uint32_t));

                if (create_desc_index == 1) {
                    auto *buffer_desc = std::get_if<BufferDesc>(&create_desc);
                    file.write(reinterpret_cast<const char *>(&buffer_desc), sizeof(BufferDesc));
                }
                else if (create_desc_index == 2) {
                    auto *image_desc = std::get_if<ImageDesc>(&create_desc);
                    file.write(reinterpret_cast<const char *>(&image_desc), sizeof(ImageDesc));
                }
                else if (create_desc_index == 3) {
                    auto *sampler_desc = std::get_if<SamplerDesc>(&create_desc);
                    file.write(reinterpret_cast<const char *>(&sampler_desc), sizeof(SamplerDesc));
                }
                file.write(reinterpret_cast<const char *>(&create_desc), sizeof(create_desc));
                file.write(name.c_str(), name.size());
                file.write("\0", 1);
            }
        }

        // entry points
        {
            const uint32_t entry_point_count = shader_data.reflection.entry_points.size();
            file.write(reinterpret_cast<const char *>(&entry_point_count), sizeof(uint32_t));
            for (const auto &[name, shader_stage, resources] : shader_data.reflection.entry_points | std::views::values) {
                //type is uint8_t
                auto buff = static_cast<uint32_t>(shader_stage);
                file.write(reinterpret_cast<const char *>(&buff), sizeof(uint32_t));
                buff = resources.size();
                file.write(reinterpret_cast<const char *>(&buff), sizeof(uint32_t));
                for (const auto &res_name : resources) {
                    file.write(res_name.c_str(), res_name.size()); file.write("\0", 1);
                }
                file.write(name.c_str(), name.size()); file.write("\0", 1);
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
            for (const auto &name : shader_data.reflection.entry_points | std::views::keys) {
                const auto code_size = shader_data.dxil_codes.at(name).size();

                file.write(reinterpret_cast<const char *>(&code_size), sizeof(uint32_t));
                file.write(shader_data.dxil_codes.at(name).data(), code_size);
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